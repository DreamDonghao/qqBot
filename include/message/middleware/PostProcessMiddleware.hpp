/// @file PostProcessMiddleware.hpp
/// @brief 消息处理完成中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 启动 Agent 后台任务，并在任务结束后发布完成事件
    class PostProcessMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 将已准备的 Agent 任务转入后台执行
        /// @param context 已完成入站记录、可能包含待执行 Agent 任务的消息上下文
        /// @return 启动任务后返回 Stop；无任务时返回 Continue
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
