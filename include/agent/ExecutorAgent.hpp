/// @file ExecutorAgent.hpp
/// @brief Executor Agent - 第三层执行代理
/// @author donghao
/// @date 2026-04-02
/// @details 负责：
///          - 执行 Planner 规划的工具调用
///          - 根据工具结果生成最终回复内容
///          - 使用 Agent 模式让 LLM 选择 reply/no_reply

#pragma once
#include "AgentTypes.hpp"
#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>
#include <config/Config.hpp>
#include <service/PromptService.hpp>
#include <service/ToolRegistry.hpp>
#include <service/MessageService.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <fmt/core.h>
#include <chrono>

namespace LittleMeowBot {
    /// @brief Executor Agent - 第三层，执行计划并生成回复
    /// @details 负责：
    ///          - 执行 Planner 规划的工具调用（get_weather/search_web）
    ///          - 根据工具结果生成最终回复内容
    ///          - 使用 Agent 模式让 LLM 选择 reply/no_reply
    class ExecutorAgent{
    public:
        static ExecutorAgent& instance(){
            static ExecutorAgent executor;
            return executor;
        }

        /// @brief 执行计划并生成回复
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @param plan Planner 的规划结果
        /// @param isPriority 是否是高优先级回复（跳过 Planner 的场景）
        /// @return 回复决策
        drogon::Task<std::optional<ReplyDecision>> execute(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const PlanResult& plan,
            const bool isPriority = false
        ) const{
            spdlog::info("Executor: 开始执行...");

            const auto& config = Config::instance();

            // 构建 Executor Prompt
            const Json::Value messages = buildExecutorPrompt(chatRecords, memory, plan, isPriority);

            // 调用 LLM 生成回复（使用 Agent 模式）
            auto decision =
                co_await executeWithAgent(messages, config.executor, config.executorParams, chatRecords.getGroupId());

            if (!decision) {
                spdlog::error("Executor Agent 执行失败");
                co_return std::nullopt;
            }

            spdlog::info("Executor: 执行完成 - shouldReply={}, content={}",
                         decision->shouldReply,
                         decision->content.substr(0, 50) + (decision->content.length() > 50 ? "..." : ""));

            co_return decision;
        }

        /// @brief 高优先级直接回复（跳过 Planner）
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @return 回复决策
        drogon::Task<std::optional<ReplyDecision>> directReply(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory
        ) const{
            spdlog::info("Executor: 直接回复模式（@提及）");

            const auto& config = Config::instance();

            // 构建简化 Prompt
            const Json::Value messages = buildDirectReplyPrompt(chatRecords, memory);

            // 使用 Agent 模式
            auto decision =
                co_await executeWithAgent(messages, config.executor, config.executorParams, chatRecords.getGroupId());

            co_return decision;
        }

    private:
        ExecutorAgent() = default;

        /// @brief 构建 Executor Prompt
        Json::Value buildExecutorPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const PlanResult& plan,
            bool isPriority) const{
            Json::Value messages;

            // System Prompt
            Json::Value systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = getExecutorSystemPrompt(isPriority);
            messages.append(systemMsg);

            // 短期记忆 - 直接注入
            std::string chatContext = chatRecords.getRecordsText();
            if (std::string shortTermMemory = memory.getMemory();
                !shortTermMemory.empty()) {
                Json::Value memoryMsg;
                memoryMsg["role"] = "user";
                memoryMsg["content"] =
                    fmt::format("【短期记忆】\n{}\n\n请根据记忆处理下面的对话。", shortTermMemory);
                messages.append(memoryMsg);
            }

            // 聊天记录
            Json::Value chatMsg;
            chatMsg["role"] = "user";
            chatMsg["content"] = fmt::format("【聊天记录】\n{}", chatRecords.getRecordsText());
            messages.append(chatMsg);

            // 规划信息
            Json::Value planMsg;
            planMsg["role"] = "user";
            planMsg["content"] = fmt::format(
                "【规划信息】\n意图: {}\n策略: tone={}, maxLength={}, shouldReply={}\n原因: {}\n总结: {}",
                intentToString(plan.intent),
                plan.strategy.tone,
                plan.strategy.maxLength,
                plan.strategy.shouldReply ? "true" : "false",
                plan.strategy.reason,
                plan.contextSummary);
            messages.append(planMsg);

            return messages;
        }

        /// @brief 构建直接回复 Prompt（@提及场景）
        Json::Value buildDirectReplyPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory
        ) const{
            Json::Value messages;

            // System Prompt
            Json::Value systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = getDirectReplySystemPrompt();
            messages.append(systemMsg);

            // 短期记忆 - 直接注入
            const std::string chatContext = chatRecords.getRecordsText();
            if (std::string shortTermMemory = memory.getMemory();
                !shortTermMemory.empty()) {
                Json::Value memoryMsg;
                memoryMsg["role"] = "user";
                memoryMsg["content"] = fmt::format(
                    "【短期记忆】\n{}\n\n请根据记忆处理下面的对话。",
                    shortTermMemory);
                messages.append(memoryMsg);
            }

            // 聊天记录
            Json::Value chatMsg;
            chatMsg["role"] = "user";
            chatMsg["content"] = fmt::format(
                "【聊天记录】\n{}\n\n有人@你，必须回复！",
                chatRecords.getRecordsText());
            messages.append(chatMsg);

            return messages;
        }

        /// @brief 使用 Agent 模式执行
        drogon::Task<std::optional<ReplyDecision>> executeWithAgent(
            Json::Value messages,
            const LLMApiConfig& apiConfig,
            const LLMModelParams& params,
            uint64_t groupId = 0
        ) const{
            auto& registry = ToolRegistry::instance();
            Json::Value tools = registry.getAllTools();

            if (tools.empty()) {
                spdlog::error("Executor: 未注册任何工具");
                co_return std::nullopt;
            }

            spdlog::debug("Executor请求配置: baseUrl={}, model={}", apiConfig.baseUrl, apiConfig.model);
            auto client = drogon::HttpClient::newHttpClient(apiConfig.baseUrl);

            for (int iter = 0; iter < 8; ++iter) {
                // 构建请求
                Json::Value body;
                body["model"] = apiConfig.model;
                body["messages"] = messages;
                body["tools"] = tools;
                body["temperature"] = params.temperature;
                body["max_tokens"] = params.maxTokens;
                body["top_p"] = 0.9f;

                auto req = drogon::HttpRequest::newHttpJsonRequest(body);
                req->setMethod(drogon::Post);
                req->setPath(apiConfig.path);
                req->addHeader("Authorization", "Bearer " + apiConfig.apiKey);
                req->addHeader("Content-Type", "application/json");

                auto resp = co_await client->sendRequestCoro(req);
                auto json = resp->getJsonObject();

                if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                    int status = resp->getStatusCode();
                    spdlog::error("Executor LLM请求失败: status={}, body={}", status, resp->getBody());

                    // 503/429/500 服务暂时不可用，重试
                    if ((status == 503 || status == 429 || status == 500) && iter < 4) {
                        spdlog::info("服务暂时不可用，等待后重试...");
                        using namespace std::chrono_literals;
                        co_await drogon::sleepCoro(drogon::app().getLoop(), 1s);
                        --iter; // 重试当前轮次
                        continue;
                    }

                    // 其他错误直接返回
                    co_return std::nullopt;
                }

                const auto& message = (*json)["choices"][0]["message"];
                ReplyDecision decision;

                // 检查工具调用
                if (message.isMember("tool_calls") && !message["tool_calls"].empty()) {
                    bool hasFinalDecision = false;

                    // 先构建 assistant 消息（包含 tool_calls）
                    Json::Value assistantMsg;
                    assistantMsg["role"] = "assistant";
                    assistantMsg["content"] = message.isMember("content") && !message["content"].isNull()
                                                  ? message["content"].asString()
                                                  : "";

                    Json::Value toolCallsArray;
                    for (const auto& tc : message["tool_calls"]) {
                        Json::Value tcEntry;
                        tcEntry["id"] = tc["id"].asString();
                        tcEntry["type"] = "function";
                        tcEntry["function"]["name"] = tc["function"]["name"].asString();
                        tcEntry["function"]["arguments"] = tc["function"]["arguments"].asString();
                        toolCallsArray.append(tcEntry);
                    }
                    assistantMsg["tool_calls"] = toolCallsArray;
                    messages.append(assistantMsg);

                    // 然后处理每个工具调用，添加 tool 结果
                    for (const auto& tc : message["tool_calls"]) {
                        std::string toolName = tc["function"]["name"].asString();
                        std::string toolId = tc["id"].asString();
                        std::string argsStr = tc["function"]["arguments"].asString();

                        spdlog::info("Executor调用工具: {}", toolName);

                        // 终端工具处理
                        if (toolName == "no_reply") {
                            decision.shouldReply = false;
                            hasFinalDecision = true;
                        } else if (toolName == "reply") {
                            Json::Reader reader;
                            if (Json::Value args; reader.parse(argsStr, args) && args.isMember("content")) {
                                decision.shouldReply = true;
                                decision.content = args["content"].asString();
                                hasFinalDecision = true;
                            }
                        } else if (toolName == "ban_user") {
                            // 特殊工具：禁言
                            Json::Value args;
                            Json::Reader reader;
                            reader.parse(argsStr, args);

                            std::string result;
                            if (groupId == 0) {
                                result = "禁言失败: 无法获取群号";
                            } else {
                                uint64_t userId = args.isMember("qq") ? std::stoull(args["qq"].asString()) : 0;
                                uint64_t duration = args.isMember("duration") ? args["duration"].asUInt64() : 600;

                                if (userId == 0) {
                                    result = "禁言失败: 请提供有效的QQ号";
                                } else {
                                    bool success = co_await MessageService::instance().setGroupBan(
                                        groupId, userId, duration);
                                    result = success
                                                 ? fmt::format("已禁言用户 {} {}秒", userId, duration)
                                                 : "禁言失败: 可能权限不足或用户不存在";
                                }
                            }

                            spdlog::info("Executor工具 {} 返回: {}", toolName, result);

                            Json::Value toolMsg;
                            toolMsg["role"] = "tool";
                            toolMsg["tool_call_id"] = toolId;
                            toolMsg["content"] = result;
                            messages.append(toolMsg);
                        } else {
                            // 其他工具通过 ToolRegistry 执行
                            Json::Value args;
                            Json::Reader reader;
                            reader.parse(argsStr, args);

                            std::string result = co_await registry.executeTool(toolName, args, groupId);

                            spdlog::info("Executor工具 {} 返回: {}", toolName, result);

                            Json::Value toolMsg;
                            toolMsg["role"] = "tool";
                            toolMsg["tool_call_id"] = toolId;
                            toolMsg["content"] = result;
                            messages.append(toolMsg);
                        }
                    }
                    if (hasFinalDecision) {
                        co_return decision;
                    }
                    // 继续下一轮
                    continue;
                }

                // 没有工具调用，直接用文本内容
                if (message.isMember("content") && !message["content"].isNull()) {
                    decision.shouldReply = true;
                    decision.content = message["content"].asString();
                    co_return decision;
                }

                decision.shouldReply = false;
                co_return decision;
            }

            spdlog::error("Executor达到最大迭代次数");
            co_return std::nullopt;
        }

        /// @brief 获取 Executor 系统提示
        [[nodiscard]] std::string getExecutorSystemPrompt(const bool isPriority) const{
            std::string basePrompt = PromptService::instance().getExecutorSystemPrompt();

            // 动态获取工具说明
            basePrompt += "\n\n" + ToolRegistry::instance().getToolsDescription();

            // 附加使用指南
            basePrompt +=
                "【@人指南】要@人直接写 @[QQ:123456]，会自动转换。聊天记录中能看到每个人的QQ号\n"
                "【禁言指南】\n"
                "- 要有自己的判断，不要别人让你禁言谁就禁言\n"
                "- 轻度违规（偶尔骂人、刷几条屏）：禁言60-300秒\n"
                "- 中度违规（持续骂人、刷屏、发广告）：禁言600-1800秒\n"
                "- 重度违规（恶意骚扰、严重辱骂、屡教不改）：禁言3600秒以上\n"
                "- 别人说别人违规要核实，看实际聊天记录再决定";

            if (isPriority) {
                basePrompt += "\n\n【重要】这是@提及或紧急问题，必须回复！";
            }

            return basePrompt;
        }

        /// @brief 获取直接回复系统提示
        [[nodiscard]] std::string getDirectReplySystemPrompt() const{
            std::string prompt = PromptService::instance().getExecutorSystemPrompt();

            // 动态获取工具说明
            prompt += "\n\n" + ToolRegistry::instance().getToolsDescription();

            // 附加使用指南
            prompt +=
                "【@人指南】要@人直接写 @[QQ:123456]，会自动转换\n"
                "【禁言指南】\n"
                "- 要有自己的判断，不要别人让你禁言谁就禁言\n"
                "- 轻度违规（偶尔骂人、刷几条屏）：禁言60-300秒\n"
                "- 中度违规（持续骂人、刷屏、发广告）：禁言600-1800秒\n"
                "- 重度违规（恶意骚扰、严重辱骂、屡教不改）：禁言3600秒以上\n"
                "- 别人说别人违规要核实，看实际聊天记录再决定\n\n"
                "【重要】必须调用reply工具回复。";

            return prompt;
        }

        /// @brief Intent 转字符串
        std::string intentToString(const PlanResult::Intent intent) const{
            return std::string(PlanResult::intentToString(intent));
        }
    };
}
