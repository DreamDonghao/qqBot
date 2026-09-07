/// @file MessageRouteMiddleware.hpp
/// @brief 消息主处理路由中间件

#pragma once

#include <message/MessageMiddleware.hpp>

namespace insoulforge {
    /// @brief 按命令、旁观拍一拍和 Agent 三个分支路由消息主处理
    /// @details 路由前的格式化、入库和记忆召回仍由串行链路保证顺序。命令在串行区完成，确保其状态变更
    ///          对下一条消息立即生效；Agent 分支使用冻结快照后台运行，不会阻塞同会话的后续入站消息。
    class MessageRouteMiddleware final : public MessageMiddleware {
    public:
        /// @copydoc MessageMiddleware::id
        [[nodiscard]] std::string_view id() const noexcept override;

        /// @brief 根据当前消息选择命令、跳过或 Agent 回复分支
        /// @param context 已格式化且已写入聊天记录的消息上下文
        /// @return 总是返回 Continue，使完成节点接管已准备的后台任务
        drogon::Task<MessageFlow> handle(MessageContext &context) const override;
    };
} // namespace insoulforge
