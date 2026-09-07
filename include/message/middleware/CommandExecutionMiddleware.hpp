/// @file CommandExecutionMiddleware.hpp
/// @brief 管理命令执行中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 在命令消息写入聊天记录后执行管理命令
    class CommandExecutionMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 执行已识别命令并终止常规 Agent 流程
        /// @param context 已格式化、已记录的命令消息上下文
        /// @return 非命令继续后续流程；命令执行后返回 Stop
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
