/// @file MessageContext.cpp
/// @brief 入站消息处理链路上下文实现

#include <message/MessageContext.hpp>
#include <message/runtime/MessageRuntime.hpp>

#include <stdexcept>

namespace insoulforge {
    MessageContext::MessageContext(json event, std::shared_ptr<const MessageRuntime> runtime) :
        m_event(std::move(event)), m_runtime(std::move(runtime)) {}

    json &MessageContext::event() { return m_event; }

    const MessageRuntime &MessageContext::runtime() const noexcept { return *m_runtime; }

    const std::shared_ptr<const MessageRuntime> &MessageContext::runtimeHandle() const noexcept { return m_runtime; }

    void MessageContext::createMessage() {
        // 事件归一化完成后不再需要原始 JSON，转移其所有权以避免复制消息段。
        m_message.emplace(std::move(m_event));
    }

    OneBotMessage &MessageContext::message() { return *m_message; }

    const OneBotMessage &MessageContext::message() const { return *m_message; }

    uint64_t MessageContext::sessionId() const { return message().getSessionId(); }

    uint64_t MessageContext::logSessionId() const {
        if (m_message) {
            return m_message->getSessionId();
        }

        const uint64_t groupId = getUInt(m_event, "group_id", 0);
        if (groupId != 0) {
            return groupId;
        }
        const uint64_t userId = getUInt(m_event, "user_id", getUInt(atOrNull(m_event, "sender"), "user_id", 0));
        return userId == 0 ? 0 : userId | OneBotMessage::kPrivateSessionFlag;
    }

    uint64_t MessageContext::logMessageId() const {
        return m_message ? m_message->getMessageId() : getUInt(m_event, "message_id", 0);
    }

    ChatRecordManager &MessageContext::chatRecords() {
        if (!m_chatRecords) {
            // 命令、格式化失败或被短路的消息无需构造记录管理器。
            m_chatRecords.emplace(sessionId());
        }
        return *m_chatRecords;
    }

    void MessageContext::markCommand() noexcept { m_isCommand = true; }

    bool MessageContext::isCommand() const noexcept { return m_isCommand; }

    void MessageContext::deferProcessingTask(drogon::Task<MessageProcessingOutcome> task) {
        m_deferredProcessingTask.emplace(std::move(task));
    }

    bool MessageContext::hasDeferredProcessingTask() const noexcept { return m_deferredProcessingTask.has_value(); }

    drogon::Task<MessageProcessingOutcome> MessageContext::takeDeferredProcessingTask() {
        if (!m_deferredProcessingTask) {
            throw std::logic_error("不存在待启动的主处理任务");
        }
        auto task = std::move(*m_deferredProcessingTask);
        m_deferredProcessingTask.reset();
        return task;
    }
} // namespace insoulforge
