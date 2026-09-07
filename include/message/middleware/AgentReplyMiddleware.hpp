/// @file AgentReplyMiddleware.hpp
/// @brief Agent 回复处理中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 在后台调用 Agent 处理消息，并按决策发送文本回复
    class AgentReplyMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 准备带聊天记录快照的 Agent 任务
        /// @param context 已格式化且已写入聊天记录的消息上下文
        /// @return 旁观拍一拍返回 Stop；其他消息将任务交由后处理节点启动后返回 Continue
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
