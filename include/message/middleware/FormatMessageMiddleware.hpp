/// @file FormatMessageMiddleware.hpp
/// @brief QQ 消息格式化中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 执行消息格式化、图片识别与本轮长期记忆召回
    class FormatMessageMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 格式化消息内容并完成本轮向量召回
        /// @param context 已通过会话启用检查的消息上下文
        /// @return 图片识别和召回完成后返回 Continue
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
