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
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <string>
#include <optional>

namespace LittleMeowBot {
    /// @brief Planner Agent - 第二层，意图分析和工具规划
    /// @details 负责：
    ///          - 分析用户意图（question/chat/help/attack/greeting）
    ///          - 规划需要的工具调用（get_weather/search_web）
    ///          - 确定回复策略（tone, maxLength, shouldReply）
    class PlannerAgent{
    public:
        static PlannerAgent& instance();

        /// @brief 规划回复策略
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @param routerDecision Router 的决策结果
        /// @return 规划结果
        drogon::Task<std::optional<PlanResult>> plan(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const RouterDecision& routerDecision) const;

    private:
        PlannerAgent() = default;

        /// @brief 构建 Planner Prompt
        Json::Value buildPlannerPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const RouterDecision& routerDecision) const;

        /// @brief 从响应中提取 JSON
        std::string extractJson(const std::string& content) const;

        /// @brief Action 转字符串
        std::string actionToString(RouterDecision::Action action) const;
    };
}
