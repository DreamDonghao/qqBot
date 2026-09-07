/// @file DomainEvent.hpp
/// @brief 领域事件的强类型载荷定义

#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace insoulforge {
    /// @brief 已支持的领域事件类别
    enum class DomainEventType : size_t {
        MessageRecorded,
        MessageProcessingCompleted,
        Count,
    };

    /// @brief 获取领域事件类别的稳定日志名称
    /// @param type 领域事件类别
    /// @return 适合日志与诊断输出的事件名称
    [[nodiscard]] constexpr std::string_view domainEventTypeName(const DomainEventType type) {
        switch (type) {
            case DomainEventType::MessageRecorded:
                return "message_recorded";
            case DomainEventType::MessageProcessingCompleted:
                return "message_processing_completed";
            case DomainEventType::Count:
                return "unknown";
        }
        return "unknown";
    }

    /// @brief 聊天记录的消息角色
    enum class MessageRole {
        User,
        Assistant,
    };

    /// @brief 转换消息角色为管理后台使用的字符串
    [[nodiscard]] constexpr std::string_view messageRoleName(const MessageRole role) {
        return role == MessageRole::User ? "user" : "assistant";
    }

    /// @brief 消息主处理树的最终分支结果
    enum class MessageProcessingOutcome {
        CommandHandled, ///< 已执行管理命令并投递结果
        CommandFailed, ///< 管理命令执行或投递失败
        PassivePokeIgnored, ///< 与机器人无关的拍一拍，不进入 Agent
        AgentSkipped, ///< Agent 主动决定不回复
        AgentBusy, ///< 会话已有普通 Agent 任务，当前消息未进入 Agent
        AgentCancelled, ///< Agent 被同会话高优先级消息抢占取消
        AgentUnavailable, ///< Agent 在处理期间不可用
        AgentFailed, ///< Agent 调用或生成回复计划失败
        ReplySent, ///< 已投递 Agent 回复
        ReplyFailed, ///< Agent 已生成回复，但投递失败
        UnexpectedFailure, ///< 工作流边界捕获到未分类异常
    };

    /// @brief 获取消息处理结果的稳定日志名称
    [[nodiscard]] constexpr std::string_view messageProcessingOutcomeName(const MessageProcessingOutcome outcome) {
        switch (outcome) {
            case MessageProcessingOutcome::CommandHandled:
                return "command_handled";
            case MessageProcessingOutcome::CommandFailed:
                return "command_failed";
            case MessageProcessingOutcome::PassivePokeIgnored:
                return "passive_poke_ignored";
            case MessageProcessingOutcome::AgentSkipped:
                return "agent_skipped";
            case MessageProcessingOutcome::AgentBusy:
                return "agent_busy";
            case MessageProcessingOutcome::AgentCancelled:
                return "agent_cancelled";
            case MessageProcessingOutcome::AgentUnavailable:
                return "agent_unavailable";
            case MessageProcessingOutcome::AgentFailed:
                return "agent_failed";
            case MessageProcessingOutcome::ReplySent:
                return "reply_sent";
            case MessageProcessingOutcome::ReplyFailed:
                return "reply_failed";
            case MessageProcessingOutcome::UnexpectedFailure:
                return "unexpected_failure";
        }
        return "unexpected_failure";
    }

    /// @brief 判断完成事件是否应触发普通对话的统计与记忆维护
    /// @details 命令与旁观拍一拍虽会发布完成事件以便审计，但不应污染对话统计和长期记忆。
    [[nodiscard]] constexpr bool isConversationProcessingOutcome(const MessageProcessingOutcome outcome) {
        return outcome != MessageProcessingOutcome::CommandHandled &&
               outcome != MessageProcessingOutcome::CommandFailed &&
               outcome != MessageProcessingOutcome::PassivePokeIgnored;
    }

    /// @brief 一条消息已写入聊天记录
    struct MessageRecordedEvent {
        static constexpr auto kType = DomainEventType::MessageRecorded;

        uint64_t sessionId; ///< 所属会话 ID
        uint64_t messageId; ///< OneBot 消息 ID
        MessageRole role; ///< 消息角色
        std::string recordContent; ///< 写入聊天记录的格式化内容
        std::string displayContent; ///< 推送给管理后台的展示内容
    };

    /// @brief 一条普通消息的主处理链路已经结束
    struct MessageProcessingCompletedEvent {
        static constexpr auto kType = DomainEventType::MessageProcessingCompleted;

        uint64_t sessionId; ///< 所属会话 ID
        uint64_t messageId; ///< OneBot 消息 ID
        size_t contentSize; ///< 格式化消息的字节长度，用于会话统计
        MessageProcessingOutcome outcome; ///< 工作流最终分支结果
    };

    /// @brief 所有领域事件载荷的封闭联合
    using DomainEvent = std::variant<MessageRecordedEvent, MessageProcessingCompletedEvent>;

    /// @brief 获取领域事件类别
    /// @param event 领域事件
    /// @return 对应的事件类别
    [[nodiscard]] constexpr DomainEventType domainEventType(const DomainEvent &event) {
        return std::visit([]<typename T0>(const T0 &) { return std::decay_t<T0>::kType; }, event);
    }

    /// @brief 获取领域事件所属会话 ID
    /// @param event 领域事件
    /// @return 事件的会话 ID
    [[nodiscard]] constexpr uint64_t domainEventSessionId(const DomainEvent &event) {
        return std::visit([](const auto &payload) { return payload.sessionId; }, event);
    }

    /// @brief 获取领域事件关联的消息 ID
    /// @param event 领域事件
    /// @return 事件的消息 ID
    [[nodiscard]] constexpr uint64_t domainEventMessageId(const DomainEvent &event) {
        return std::visit([](const auto &payload) { return payload.messageId; }, event);
    }
} // namespace insoulforge
