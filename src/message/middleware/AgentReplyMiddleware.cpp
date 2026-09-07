/// @file AgentReplyMiddleware.cpp
/// @brief Agent 回复处理中间件实现

#include <deque>
#include <exception>
#include <message/MessageContext.hpp>
#include <message/middleware/AgentReplyMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <model/OneBotMessage.hpp>
#include <service/ChatRecordManager.hpp>
#include <service/MemoryManager.hpp>
#include <util/Logger.hpp>
#include <utility>

namespace insoulforge {
    namespace {
        /// @brief 执行与入站队列解耦的 Agent 主处理阶段
        /// @details records 在任务创建时冻结，因此后续入站消息不会改变 Router 或 Executor 的会话上下文。
        drogon::Task<> processAgentTask(std::shared_ptr<const MessageRuntime> runtime, OneBotMessage message,
          std::deque<json> records) {
            const uint64_t sessionId = message.getSessionId();
            const auto log = Logger::session(sessionId);
            ChatRecordManager chatRecords(sessionId, std::move(records));
            MemoryManager memory(sessionId);

            try {
                if (auto result = co_await runtime->processAgent(chatRecords, memory, message);
                    result && !result->empty() && runtime->isAgentRunning()) {
                    log.info("多层代理决定回复");
                    co_await runtime->sendReply(message, chatRecords, std::move(*result));
                } else {
                    log.info("多层代理决定不回复");
                }
            } catch (const std::exception &error) {
                log.error("消息处理异常: {}", error.what());
            } catch (...) {
                log.error("消息处理异常: 未知异常");
            }
        }
    } // namespace

    std::string_view AgentReplyMiddleware::id() const noexcept { return "agent_reply"; }

    drogon::Task<MessageFlow> AgentReplyMiddleware::handle(MessageContext &context) const {
        if (context.message().isPassivePoke()) {
            co_return MessageFlow::Stop;
        }

        auto runtime = context.runtimeHandle();
        auto message = context.message();
        auto records = context.chatRecords().getRecords();
        context.deferAgentTask(processAgentTask(std::move(runtime), std::move(message), std::move(records)));
        co_return MessageFlow::Continue;
    }
} // namespace insoulforge
