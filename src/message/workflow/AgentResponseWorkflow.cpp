/// @file AgentResponseWorkflow.cpp
/// @brief Agent 决策与回复投递分支实现

#include <exception>
#include <message/runtime/MessageRuntime.hpp>
#include <message/workflow/AgentResponseWorkflow.hpp>
#include <service/ChatRecordManager.hpp>
#include <service/MemoryManager.hpp>
#include <util/Logger.hpp>
#include <utility>

namespace insoulforge {
    namespace {
        /// @brief 将 Agent 主处理结果映射为消息工作流的非回复终点
        [[nodiscard]] MessageProcessingOutcome mapAgentOutcome(const AgentProcessResult::Outcome outcome) {
            switch (outcome) {
                case AgentProcessResult::Outcome::Skipped:
                    return MessageProcessingOutcome::AgentSkipped;
                case AgentProcessResult::Outcome::Busy:
                    return MessageProcessingOutcome::AgentBusy;
                case AgentProcessResult::Outcome::Cancelled:
                    return MessageProcessingOutcome::AgentCancelled;
                case AgentProcessResult::Outcome::Unavailable:
                    return MessageProcessingOutcome::AgentUnavailable;
                case AgentProcessResult::Outcome::Failed:
                case AgentProcessResult::Outcome::Reply:
                    return MessageProcessingOutcome::AgentFailed;
            }
            return MessageProcessingOutcome::AgentFailed;
        }
    } // namespace

    drogon::Task<MessageProcessingOutcome> AgentResponseWorkflow::execute(
      std::shared_ptr<const MessageRuntime> runtime, OneBotMessage message, std::deque<json> records) {
        const uint64_t sessionId = message.getSessionId();
        const auto log = Logger::session(sessionId);
        ChatRecordManager chatRecords(sessionId, std::move(records));
        MemoryManager memory(sessionId);

        try {
            AgentProcessResult result = co_await runtime->processAgent(chatRecords, memory, message);
            if (result.outcome != AgentProcessResult::Outcome::Reply) {
                const MessageProcessingOutcome outcome = mapAgentOutcome(result.outcome);
                log.info("Agent 分支结束: {}", messageProcessingOutcomeName(outcome));
                co_return outcome;
            }
            if (result.content.empty()) {
                log.error("Agent 返回 Reply 结果但回复内容为空");
                co_return MessageProcessingOutcome::AgentFailed;
            }

            try {
                co_await runtime->sendReply(message, chatRecords, std::move(result.content));
                log.info("Agent 回复已投递");
                co_return MessageProcessingOutcome::ReplySent;
            } catch (const std::exception &error) {
                log.error("Agent 回复投递失败: {}", error.what());
            } catch (...) {
                log.error("Agent 回复投递失败: 未知异常");
            }
            co_return MessageProcessingOutcome::ReplyFailed;
        } catch (const std::exception &error) {
            log.error("Agent 决策失败: {}", error.what());
        } catch (...) {
            log.error("Agent 决策失败: 未知异常");
        }
        co_return MessageProcessingOutcome::AgentFailed;
    }
} // namespace insoulforge
