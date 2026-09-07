/// @file CommandDetectionMiddleware.hpp
/// @brief 管理命令识别中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 识别命令消息
    class CommandDetectionMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 标记命令消息，实际执行交由 CommandExecutionMiddleware
        /// @param context 已创建 OneBotMessage 的消息上下文
        /// @return 始终返回 Continue
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
