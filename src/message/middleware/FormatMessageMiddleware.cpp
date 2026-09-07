/// @file FormatMessageMiddleware.cpp
/// @brief QQ 消息格式化中间件实现

#include <message/MessageContext.hpp>
#include <message/middleware/FormatMessageMiddleware.hpp>
#include <service/MessageRecall.hpp>

namespace insoulforge {
    std::string_view FormatMessageMiddleware::id() const noexcept { return "format_message"; }

    drogon::Task<MessageFlow> FormatMessageMiddleware::handle(MessageContext &context) const {
        co_await context.message().enrichContent();
        const bool isAssistant = context.message().getSelfQQNumber() == context.message().getSenderQQNumber();
        co_await MessageRecall::recallForRecord(context.sessionId(), context.message().recordContent(), isAssistant);
        co_return MessageFlow::Continue;
    }
} // namespace insoulforge
