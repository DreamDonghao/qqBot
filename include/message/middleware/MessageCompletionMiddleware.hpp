/// @file MessageCompletionMiddleware.hpp
/// @brief 消息主处理统一收尾中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 启动路由任务，并在任一分支结束后发布统一完成事件
    class MessageCompletionMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 将已准备的分支任务移交后台，并终止入站串行链路
        /// @param context 已由 MessageRouteMiddleware 准备主处理任务的上下文
        /// @return 启动后台任务后返回 Stop
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
