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
#include <model/QQMessage.hpp>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <string>
#include <optional>

namespace LittleMeowBot {
    /// @brief Router Agent - 第一层，快速路由决策
    /// @details 负责快速判断消息是否需要处理：
    ///          - 硬规则检查（冷却、刷屏、@提及）- 无需 LLM
    ///          - LLM 辅助判断（意图、话题参与）
    class RouterAgent{
    public:
        static RouterAgent& instance();

        /// @brief 路由决策
        /// @param chatRecords 聊天记录管理器
        /// @param message QQ 消息
        /// @return 路由决策结果
        drogon::Task<RouterDecision> route(
            const ChatRecordManager& chatRecords,
            const QQMessage& message) const;

    private:
        RouterAgent() = default;

        /// @brief 刷屏检测 - 检查是否是纯表情/无意义消息
        [[nodiscard]] bool checkSpam(const QQMessage& message) const;

        /// @brief LLM 辅助路由判断（使用轻量模型）
        drogon::Task<std::optional<RouterDecision>> llmRoute(const ChatRecordManager& chatRecords) const;

        /// @brief 构建 Router Prompt
        [[nodiscard]] Json::Value buildRouterPrompt(const ChatRecordManager& chatRecords) const;

        /// @brief 解析 Router LLM 响应
        [[nodiscard]] std::optional<RouterDecision> parseRouterResponse(const std::string& content) const;
    };
}
