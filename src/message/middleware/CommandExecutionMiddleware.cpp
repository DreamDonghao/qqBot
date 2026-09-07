/// @file CommandExecutionMiddleware.cpp
/// @brief 管理命令执行中间件实现

#include <controllers/CommandHandler.hpp>
#include <message/MessageContext.hpp>
#include <message/middleware/CommandExecutionMiddleware.hpp>
#include <util/Logger.hpp>

namespace insoulforge {
    std::string_view CommandExecutionMiddleware::id() const noexcept { return "command_execution"; }

    drogon::Task<MessageFlow> CommandExecutionMiddleware::handle(MessageContext &context) const {
        if (!context.isCommand()) {
            co_return MessageFlow::Continue;
        }
        Logger::session(context.sessionId()).info("收到命令消息: {}", context.message().getRawMessage());
        co_await context.sendReply(co_await handleCommand(context.message()));
        co_return MessageFlow::Stop;
    }
} // namespace insoulforge
