/// @file AgentResponseWorkflow.hpp
/// @brief Agent 决策与回复投递分支

#pragma once

#include <deque>
#include <memory>

#include <drogon/utils/coroutine.h>
#include <event/DomainEvent.hpp>
#include <model/OneBotMessage.hpp>
#include <util/JsonUtil.hpp>

namespace insoulforge {
    class MessageRuntime;

    namespace AgentResponseWorkflow {
        /// @brief 执行 Agent 决策，并按决策选择跳过或投递回复分支
        /// @param runtime 可跨后台任务持有的消息运行时
        /// @param message 已冻结的格式化消息
        /// @param records 已冻结的会话记录快照
        /// @return 消息处理树的 Agent 分支结果
        /// @details 本函数不会读取后续入站消息；回复投递失败会返回 ReplyFailed 而不会吞没完成事件。
        drogon::Task<MessageProcessingOutcome> execute(
          std::shared_ptr<const MessageRuntime> runtime, OneBotMessage message, std::deque<json> records);
    } // namespace AgentResponseWorkflow
} // namespace insoulforge
