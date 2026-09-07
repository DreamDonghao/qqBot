/// @file AgentTypes.hpp
/// @brief Agent 类型定义 - 两层代理架构的核心数据结构
/// @details 定义两层代理架构中使用的核心数据类型：
///          - RouterDecision: Router Agent 的路由决策（含回复策略）
///          - ReplyDecision: Executor Agent 的回复结果

#pragma once
#include <array>
#include <fmt/core.h>
#include <format>
#include <string>
#include <string_view>

namespace insoulforge {
    /// @brief Agent 主处理的结构化结果
    /// @details 将路由跳过、会话繁忙、抢占取消与回复计划区分开，调用方无需再通过空字符串推断分支。
    struct AgentProcessResult {
        /// @brief Agent 主处理结束原因
        enum class Outcome {
            Reply, ///< 已生成待发送的有效回复文本
            Skipped, ///< Router 或 Executor 决定不回复
            Busy, ///< 普通消息到达时，该会话已有 Agent 正在处理
            Cancelled, ///< 被同会话的高优先级消息抢占取消
            Unavailable, ///< Agent 未运行或尚未完成初始化
            Failed, ///< Executor 未能生成有效回复
        };

        Outcome outcome{Outcome::Skipped}; ///< 决定后续消息路由的结果类别
        std::string content; ///< outcome 为 Reply 时待发送的回复文本
    };

    /// @brief Router Agent 决策结果（合并了规划功能）
    struct RouterDecision {
        enum class Action {
            SKIP, ///< 不处理
            REPLY ///< 需要回复
        };

        Action action = Action::SKIP;
        std::string reason;

        // 回复策略
        bool shouldReply = true;
        std::string tone = "friendly";
        int maxLength = 25;
        bool isPriority = false;
        bool isPrivate = false; ///< 是否私聊会话（决定 Executor 使用私聊人设提示词）

        [[nodiscard]] static constexpr std::string_view actionToString(Action a) {
            constexpr std::array names = {"skip", "reply"};
            return names[static_cast<size_t>(a)];
        }
    };

    /// @brief Executor Agent 回复结果
    struct ReplyDecision {
        bool shouldReply = false;
        std::string content;
    };
} // namespace insoulforge

// fmt::formatter 特化
template<>
struct fmt::formatter<insoulforge::RouterDecision::Action> : formatter<string_view> {
    template<typename FormatContext>
    auto format(const insoulforge::RouterDecision::Action a, FormatContext &ctx) const {
        return formatter<string_view>::format(insoulforge::RouterDecision::actionToString(a), ctx);
    }
};

// std::formatter 特化
template<>
// 标准 C++20 定制点：为用户类型特化 std::formatter（cert-dcl58-cpp 误报，显式豁免）
struct std::formatter<insoulforge::RouterDecision::Action>
    : std::formatter<std::string_view> { // NOLINT(cert-dcl58-cpp)
    template<typename FormatContext>
    auto format(const insoulforge::RouterDecision::Action a, FormatContext &ctx) const {
        return std::formatter<std::string_view>::format(insoulforge::RouterDecision::actionToString(a), ctx);
    }
};
