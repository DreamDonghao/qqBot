/// @file PostProcessMiddleware.cpp
/// @brief 消息处理完成中间件实现

#include <event/DomainEvent.hpp>
#include <message/MessageContext.hpp>
#include <message/middleware/PostProcessMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <utility>

namespace insoulforge {
    std::string_view PostProcessMiddleware::id() const noexcept { return "post_process"; }

    drogon::Task<MessageFlow> PostProcessMiddleware::handle(MessageContext &context) const {
        if (!context.hasDeferredAgentTask()) {
            co_return MessageFlow::Continue;
        }

        const MessageProcessingCompletedEvent event{
          .sessionId = context.sessionId(),
          .messageId = context.message().getMessageId(),
          .contentSize = context.message().recordContent().size(),
        };
        auto runtime = context.runtimeHandle();
        auto agentTask = context.takeDeferredAgentTask();
        drogon::async_run([runtime = std::move(runtime), event, agentTask = std::move(agentTask)]() mutable
                            -> drogon::Task<> {
            co_await std::move(agentTask);
            co_await runtime->publish(event);
        });
        co_return MessageFlow::Stop;
    }
} // namespace insoulforge
