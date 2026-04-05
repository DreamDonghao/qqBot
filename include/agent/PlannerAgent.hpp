/// @file PlannerAgent.hpp
/// @brief Planner Agent - 第二层意图分析和策略规划代理
/// @author donghao
/// @date 2026-04-02
/// @details 负责：
///          - 分析用户意图（question/chat/help/attack/greeting）
///          - 规划需要的工具调用（get_weather/search_web）
///          - 确定回复策略（tone, maxLength, shouldReply）

#pragma once
#include "AgentTypes.hpp"
#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>
#include <config/Config.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <fmt/core.h>

namespace LittleMeowBot {
    /// @brief Planner Agent - 第二层，意图分析和工具规划
    /// @details 负责：
    ///          - 分析用户意图（question/chat/help/attack/greeting）
    ///          - 规划需要的工具调用（get_weather/search_web）
    ///          - 确定回复策略（tone, maxLength, shouldReply）
    class PlannerAgent{
    public:
        static PlannerAgent& instance(){
            static PlannerAgent planner;
            return planner;
        }

        /// @brief 规划回复策略
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @param routerDecision Router 的决策结果
        /// @return 规划结果
        drogon::Task<std::optional<PlanResult>> plan(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const RouterDecision& routerDecision
        ) const{
            spdlog::info("Planner: 开始规划...");

            const auto& config = Config::instance();

            // 构建 Planner Prompt
            Json::Value messages = buildPlannerPrompt(chatRecords, memory, routerDecision);

            // 使用 Planner 专用配置
            spdlog::debug("Planner请求配置: baseUrl={}, model={}", config.planner.baseUrl, config.planner.model);
            auto client = drogon::HttpClient::newHttpClient(config.planner.baseUrl);
            Json::Value body;
            body["model"] = config.planner.model;
            body["messages"] = messages;
            body["temperature"] = config.plannerParams.temperature;
            body["max_tokens"] = config.plannerParams.maxTokens;
            body["top_p"] = 0.9f;

            auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath(config.planner.path);
            req->addHeader("Authorization", "Bearer " + config.planner.apiKey);
            req->addHeader("Content-Type", "application/json");

            auto resp = co_await client->sendRequestCoro(req);
            auto json = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                spdlog::error("Planner LLM请求失败: status={}, body={}", resp->getStatusCode(), resp->getBody());
                co_return std::nullopt;
            }

            std::string content = (*json)["choices"][0]["message"]["content"].asString();
            spdlog::info("Planner LLM响应: {}", content);

            // 解析 JSON 结果
            auto planResult = PlanResult::fromJson(extractJson(content));

            if (!planResult) {
                spdlog::error("Planner JSON解析失败");
                // 返回默认计划
                PlanResult defaultPlan;
                defaultPlan.intent = PlanResult::Intent::CHAT;
                defaultPlan.strategy.shouldReply = true;
                defaultPlan.strategy.maxLength = 25;
                defaultPlan.strategy.tone = "friendly";
                defaultPlan.strategy.reason = "无法解析规划结果，使用默认策略";
                co_return defaultPlan;
            }

            spdlog::info("Planner: 规划完成 - intent={}, shouldReply={}",
                         planResult->intent,
                         planResult->strategy.shouldReply);

            co_return planResult;
        }

    private:
        PlannerAgent() = default;

        /// @brief 构建 Planner Prompt
        Json::Value buildPlannerPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const RouterDecision& routerDecision
        ) const{
            Json::Value messages;

            // System Prompt
            Json::Value systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = R"(你是消息规划器，分析用户意图并规划回复策略。

分析内容：
1. 用户意图类型：QUESTION(提问)/CHAT(闲聊)/HELP(求助)/ATTACK(攻击)/GREETING(问候)/UNKNOWN(未知)
2. 回复策略：shouldReply(true/false), tone(语气), maxLength(字数限制)

输出格式（JSON）：
{
  "intent": "QUESTION/CHAT/HELP/ATTACK/GREETING/UNKNOWN",
  "strategy": {"shouldReply": true, "tone": "friendly/sarcastic/helpful", "maxLength": 25},
  "context_summary": "简短上下文总结"
}

重要：只输出JSON，简洁分析。)";
            messages.append(systemMsg);

            // 聊天记录
            std::string chatText = chatRecords.getRecordsText();

            // 短期记忆 - 直接注入
            if (std::string shortTermMemory = memory.getMemory();
                !shortTermMemory.empty()
            ) {
                Json::Value memoryMsg;
                memoryMsg["role"] = "user";
                memoryMsg["content"] = fmt::format(
                    "【短期记忆】\n{}\n请根据记忆处理下面的对话。",
                    shortTermMemory);
                messages.append(memoryMsg);
            }

            Json::Value chatMsg;
            chatMsg["role"] = "user";
            chatMsg["content"] = fmt::format(
                "【聊天记录】\n{}\nRouter决策: {} ({})",
                chatText,
                actionToString(routerDecision.action),
                routerDecision.reason);
            messages.append(chatMsg);

            return messages;
        }

        /// @brief 从响应中提取 JSON
        std::string extractJson(const std::string& content) const{
            std::string jsonStr = content;

            // 尝试找到 JSON 部分
            const size_t start = jsonStr.find('{');
            if (const size_t end = jsonStr.rfind('}');
                start != std::string::npos && end != std::string::npos) {
                jsonStr = jsonStr.substr(start, end - start + 1);
            }

            return jsonStr;
        }

        /// @brief Action 转字符串
        std::string actionToString(RouterDecision::Action action) const{
            return std::string(RouterDecision::actionToString(action));
        }
    };
}
