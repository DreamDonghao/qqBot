/// @file CommandDetectionMiddleware.cpp
/// @brief 管理命令识别中间件实现

#include <controllers/CommandHandler.hpp>
#include <message/MessageContext.hpp>
#include <message/middleware/CommandDetectionMiddleware.hpp>

namespace insoulforge {
    std::string_view CommandDetectionMiddleware::id() const noexcept { return "command_detection"; }

    drogon::Task<MessageFlow> CommandDetectionMiddleware::handle(MessageContext &context) const {
        if (isCommand(context.message())) {
            context.markCommand();
        }
        co_return MessageFlow::Continue;
    }
} // namespace insoulforge
