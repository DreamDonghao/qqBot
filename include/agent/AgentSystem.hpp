/// @file AgentSystem.hpp
/// @brief Agent 系统 - 多层代理架构整合
/// @author donghao
/// @date 2026-04-02
/// @details 协调三层代理流程处理 QQ 消息：
///          - Layer 1 (Router): 快速判断是否需要回复
///          - Layer 2 (Planner): 分析意图并规划回复策略
///          - Layer 3 (Executor): 执行回复并调用工具
///
///          特殊路径：
///          - Router SKIP → 直接结束，不回复
///          - Router PRIORITY_REPLY → 跳过 Planner，直接进入 Executor

#pragma once
#include "AgentToolManager.hpp"
#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>
#include <model/QQMessage.hpp>
#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <mutex>

namespace LittleMeowBot {
    /// @brief Agent 系统单例类
    /// @details 协调三层代理流程，提供统一的消息处理接口
    class AgentSystem{
    public:
        static AgentSystem& instance();

        /// @brief 初始化 Agent System（注册工具）
        void initialize();

        /// @brief 处理消息 - 主流程
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @param message QQ 消息
        /// @return 回复内容（如果需要回复）
        drogon::Task<std::optional<std::string>> process(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const QQMessage& message);

        /// @brief 处理 @提及消息 - 直接回复路径
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @return 回复内容
        drogon::Task<std::optional<std::string>> processAtMention(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory) const;

    private:
        AgentSystem() = default;
        bool m_initialized = false;

        // 正在处理中的群聊（防止同时处理多条消息）
        std::unordered_set<uint64_t> m_processingGroups;
        std::mutex m_processingMutex;

        /// @brief 检查群是否正在处理
        bool isProcessing(uint64_t groupId);

        /// @brief 标记群为处理中
        void markProcessing(uint64_t groupId);

        /// @brief 标记群处理完成
        void unmarkProcessing(uint64_t groupId);
    };
} // namespace LittleMeowBot
