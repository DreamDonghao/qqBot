/// @file MessageRouteMiddleware.cpp
/// @brief 消息主处理路由中间件实现

#include <controllers/CommandHandler.hpp>
#include <exception>
#include <message/MessageContext.hpp>
#include <message/middleware/MessageRouteMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <message/workflow/AgentResponseWorkflow.hpp>
#include <service/ChatRecordManager.hpp>
#include <util/Logger.hpp>
#include <utility>

namespace insoulforge {
    namespace {
        /// @brief 执行命令分支并返回可供统一收尾发布的结果
        drogon::Task<MessageProcessingOutcome> executeCommand(
          std::shared_ptr<const MessageRuntime> runtime, OneBotMessage message, std::deque<json> records) {
            const uint64_t sessionId = message.getSessionId();
            const auto log = Logger::session(sessionId);
            ChatRecordManager chatRecords(sessionId, std::move(records));

            try {
                std::string response = co_await handleCommand(message);
                co_await runtime->sendReply(message, chatRecords, std::move(response));
                co_return MessageProcessingOutcome::CommandHandled;
            } catch (const std::exception &error) {
                log.error("命令分支执行失败: {}", error.what());
            } catch (...) {
                log.error("命令分支执行失败: 未知异常");
            }
            co_return MessageProcessingOutcome::CommandFailed;
        }

        /// @brief 返回无需 Agent 处理的旁观拍一拍结果
        drogon::Task<MessageProcessingOutcome> ignorePassivePoke() {
            co_return MessageProcessingOutcome::PassivePokeIgnored;
        }

        /// @brief 将已在串行区执行完的分支结果交给统一完成节点
        drogon::Task<MessageProcessingOutcome> complete(const MessageProcessingOutcome outcome) { co_return outcome; }
    } // namespace

    std::string_view MessageRouteMiddleware::id() const noexcept { return "message_route"; }

    drogon::Task<MessageFlow> MessageRouteMiddleware::handle(MessageContext &context) const {
        auto runtime = context.runtimeHandle();
        auto message = context.message();

        if (context.isCommand()) {
            Logger::session(context.sessionId()).info("消息路由: command");
            // 命令可能变更会话开关等后续消息依赖的状态，必须在当前会话 FIFO 中完成。
            const MessageProcessingOutcome outcome =
              co_await executeCommand(runtime, message, context.chatRecords().getRecords());
            context.deferProcessingTask(complete(outcome));
        } else if (message.isPassivePoke()) {
            Logger::session(context.sessionId()).debug("消息路由: passive_poke");
            context.deferProcessingTask(ignorePassivePoke());
        } else {
            Logger::session(context.sessionId()).debug("消息路由: agent");
            context.deferProcessingTask(AgentResponseWorkflow::execute(
              std::move(runtime), std::move(message), context.chatRecords().getRecords()));
        }
        co_return MessageFlow::Continue;
    }
} // namespace insoulforge
