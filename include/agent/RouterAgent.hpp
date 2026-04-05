/// @file RouterAgent.hpp
/// @brief Router Agent - 第一层路由决策代理
/// @author donghao
/// @date 2026-04-02
/// @details 负责快速判断消息是否需要处理：
///          - 硬规则检查（冷却、刷屏、@提及）- 无需 LLM
///          - LLM 辅助判断（意图、话题参与）
///          决策结果：
///          - SKIP: 不处理，跳过
///          - PRIORITY_REPLY: 高优先级回复（@提及、紧急求助）
///          - NORMAL_PROCESS: 正常处理，进入 Planner

#pragma once
#include "AgentTypes.hpp"
#include <model/ChatRecordManager.hpp>
#include <config/Config.hpp>
#include <model/QQMessage.hpp>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <regex>

namespace LittleMeowBot {
    /// @brief Router Agent - 第一层，快速路由决策
    /// @details 负责快速判断消息是否需要处理：
    ///          - 硬规则检查（冷却、刷屏、@提及）- 无需 LLM
    ///          - LLM 辅助判断（意图、话题参与）
    class RouterAgent{
    public:
        static RouterAgent& instance(){
            static RouterAgent router;
            return router;
        }

        /// @brief 路由决策
        /// @param chatRecords 聊天记录管理器
        /// @param message QQ 消息
        /// @return 路由决策结果
        drogon::Task<RouterDecision> route(
            const ChatRecordManager& chatRecords,
            const QQMessage& message
        ) const{
            // Step 1: 硬规则检查（无需 LLM）
            // 1.1 @提及检测
            if (message.atMe()) {
                spdlog::info("Router: @提及检测 → PRIORITY_REPLY");
                RouterDecision decision;
                decision.action = RouterDecision::Action::PRIORITY_REPLY;
                decision.reason = "用户@提及" + Config::instance().botName;
                co_return decision;
            }

            // 1.2 刷屏检测
            if (checkSpam(message)) {
                spdlog::info("Router: 刷屏消息 → SKIP");
                RouterDecision decision;
                decision.action = RouterDecision::Action::SKIP;
                decision.reason = "刷屏/纯表情消息";
                co_return decision;
            }

            // 1.4 自身消息检测
            if (message.getSelfQQNumber() == message.getSenderQQNumber()) {
                spdlog::info("Router: 自身消息 → SKIP");
                RouterDecision decision;
                decision.action = RouterDecision::Action::SKIP;
                decision.reason = "自身消息";
                co_return decision;
            }

            // Step 2: LLM 辅助判断（使用轻量模型）
            auto llmDecision = co_await llmRoute(chatRecords);

            if (!llmDecision) {
                // LLM 失败时默认正常处理
                RouterDecision decision;
                decision.action = RouterDecision::Action::NORMAL_PROCESS;
                decision.reason = "LLM路由失败，默认正常处理";
                co_return decision;
            }

            co_return llmDecision.value();
        }

    private:
        RouterAgent() = default;

        /// @brief 刷屏检测 - 检查是否是纯表情/无意义消息
        [[nodiscard]] bool checkSpam(const QQMessage& message) const{
            std::string rawMsg = message.getRawMessage();
            // 去除空白字符
            std::erase_if(rawMsg, [](const char c) { return std::isspace(c); });
            // 空消息
            if (rawMsg.empty()) return true;
            // 纯表情（QQ表情通常很短或包含特殊标记）
            if (rawMsg.length() <= 2) return true;
            // 纯表情包标记
            if (rawMsg == "[表情]" || rawMsg.find("[CQ:face") != std::string::npos) {
                // 检查是否全是表情
                if (const std::regex facePattern("^\\[CQ:face.*\\]$");
                    std::regex_match(rawMsg, facePattern)
                ) {
                    return true;
                }
            }

            return false;
        }

        /// @brief LLM 辅助路由判断（使用轻量模型）
        drogon::Task<std::optional<RouterDecision>> llmRoute(const ChatRecordManager& chatRecords) const{
            const auto& config = Config::instance();

            // 构建 Router Prompt
            const Json::Value messages = buildRouterPrompt(chatRecords);

            // 使用 Router 专用配置（轻量模型）
            spdlog::debug("Router请求配置: baseUrl={}, model={}", config.router.baseUrl, config.router.model);
            const auto client = drogon::HttpClient::newHttpClient(config.router.baseUrl);
            Json::Value body;
            body["model"] = config.router.model;
            body["messages"] = messages;
            body["temperature"] = config.routerParams.temperature;
            body["max_tokens"] = config.routerParams.maxTokens;
            body["top_p"] = 0.9f;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath(config.router.path);
            req->addHeader("Authorization", "Bearer " + config.router.apiKey);
            req->addHeader("Content-Type", "application/json");

            const auto resp = co_await client->sendRequestCoro(req);
            const auto json = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                spdlog::error("Router LLM请求失败: status={}, body={}", resp->getStatusCode(), resp->getBody());
                co_return std::nullopt;
            }

            std::string content = (*json)["choices"][0]["message"]["content"].asString();
            spdlog::info("Router LLM响应: {}", content);

            // 解析 JSON 结果
            auto decision = parseRouterResponse(content);
            co_return decision;
        }

        /// @brief 构建 Router Prompt
        [[nodiscard]] Json::Value buildRouterPrompt(const ChatRecordManager& chatRecords) const{
            Json::Value messages;

            // System Prompt
            Json::Value systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = R"(你是一个消息路由器，判断消息是否需要机器人回复。

判断规则(只判断最后一条消息，上文消息用于理解)：
1. SKIP（跳过）：机器人自己的消息、纯表情、无意义重复
2. PRIORITY_REPLY（优先回复）：@机器人、紧急求助、直接提问
3. NORMAL_PROCESS（正常处理）：普通群聊、闲聊内容

输出格式（JSON）：
{"action": "SKIP/PRIORITY_REPLY/NORMAL_PROCESS", "reason": "简短原因"}

重要：只输出JSON，不要其他内容。)";
            messages.append(systemMsg);

            // User Prompt - 最近聊天记录
            Json::Value userMsg;
            userMsg["role"] = "user";

            // 获取最近5条聊天记录
            std::string recentChat;
            const auto& records = chatRecords.getRecords();
            const int startIdx = std::max(0, static_cast<int>(records.size()) - 5);
            for (int i = startIdx; i < records.size(); ++i) {
                if (const auto& record = records[i];
                    record["role"].asString() == "assistant") {
                    recentChat += "{机器人(自己)}:" + record["content"].asString() + "\n";
                }
                else {
                    recentChat += record["content"].asString() + "\n";
                }
            }

            userMsg["content"] = "最近聊天记录：\n" + recentChat + "\n请判断是否需要回复。";
            messages.append(userMsg);

            return messages;
        }

        /// @brief 解析 Router LLM 响应
        [[nodiscard]] std::optional<RouterDecision> parseRouterResponse(const std::string& content) const{
            // 提取 JSON
            std::string jsonStr = content;

            // 尝试找到 JSON 部分
            size_t start = jsonStr.find('{');
            if (size_t end = jsonStr.rfind('}');
                start != std::string::npos && end != std::string::npos
            ) {
                jsonStr = jsonStr.substr(start, end - start + 1);
            }

            Json::Value root;
            if (Json::Reader reader;
                !reader.parse(jsonStr, root)
            ) {
                spdlog::error("Router JSON解析失败: {}", jsonStr);
                return std::nullopt;
            }

            RouterDecision decision;

            // 解析 action
            if (root.isMember("action")) {
                std::string actionStr = root["action"].asString();
                std::ranges::transform(actionStr, actionStr.begin(), ::tolower);
                if (actionStr == "skip") {
                    decision.action = RouterDecision::Action::SKIP;
                }
                else if (actionStr == "priority_reply") {
                    decision.action = RouterDecision::Action::PRIORITY_REPLY;
                }
                else if (actionStr == "normal_process") {
                    decision.action = RouterDecision::Action::NORMAL_PROCESS;
                }
                else {
                    decision.action = RouterDecision::Action::NORMAL_PROCESS;
                }
            }

            // 解析 reason
            if (root.isMember("reason")) {
                decision.reason = root["reason"].asString();
            }

            return decision;
        }
    };
}
