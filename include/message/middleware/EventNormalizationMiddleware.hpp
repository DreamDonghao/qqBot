/// @file EventNormalizationMiddleware.hpp
/// @brief OneBot 事件归一化中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 过滤无关事件，并将支持的 OneBot 通知归一化为富消息段
    class EventNormalizationMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 将可处理事件转换为消息事件
        /// @param context 仅包含原始 OneBot 事件的消息上下文
        /// @return 普通消息、拍一拍、入群或退群通知返回 Continue；其他事件返回 Stop
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
