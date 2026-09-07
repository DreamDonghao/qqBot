/// @file MessageCompletionMiddleware.cpp
/// @brief 消息主处理统一收尾中间件实现

#include <event/DomainEvent.hpp>
#include <exception>
#include <message/MessageContext.hpp>
#include <message/middleware/MessageCompletionMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <util/Logger.hpp>
#include <utility>

namespace insoulforge {
    std::string_view MessageCompletionMiddleware::id() const noexcept { return "message_completion"; }

    drogon::Task<MessageFlow> MessageCompletionMiddleware::handle(MessageContext &context) const {
        if (!context.hasDeferredProcessingTask()) {
            Logger::session(context.sessionId()).error("消息收尾节点未收到主处理任务");
            co_return MessageFlow::Stop;
        }

        const uint64_t sessionId = context.sessionId();
        const uint64_t messageId = context.message().getMessageId();
        const size_t contentSize = context.message().recordContent().size();
        auto runtime = context.runtimeHandle();
        auto processingTask = context.takeDeferredProcessingTask();
        drogon::async_run([runtime = std::move(runtime), processingTask = std::move(processingTask), sessionId,
                            messageId, contentSize]() mutable -> drogon::Task<> {
            MessageProcessingOutcome outcome = MessageProcessingOutcome::UnexpectedFailure;
            try {
                outcome = co_await std::move(processingTask);
            } catch (const std::exception &error) {
                Logger::session(sessionId).error("消息主处理发生未分类异常: {}", error.what());
            } catch (...) {
                Logger::session(sessionId).error("消息主处理发生未分类异常");
            }

            Logger::session(sessionId).info("消息主处理完成: {}", messageProcessingOutcomeName(outcome));
            co_await runtime->publish(MessageProcessingCompletedEvent{
              .sessionId = sessionId,
              .messageId = messageId,
              .contentSize = contentSize,
              .outcome = outcome,
            });
        });
        co_return MessageFlow::Stop;
    }
} // namespace insoulforge
