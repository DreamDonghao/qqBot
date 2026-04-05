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

#include "AgentTypes.hpp"
#include "AgentToolManager.hpp"
#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>
#include <service/PromptService.hpp>
#include "RouterAgent.hpp"
#include "PlannerAgent.hpp"
#include "ExecutorAgent.hpp"
#include <model/QQMessage.hpp>
#include <drogon/utils/coroutine.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <mutex>

namespace LittleMeowBot {
    /// @brief Agent 系统单例类
    /// @details 协调三层代理流程，提供统一的消息处理接口
    class AgentSystem {
    public:
        static AgentSystem& instance(){
            static AgentSystem system;
            return system;
        }

        /// @brief 初始化 Agent System（注册工具）
        void initialize(){
            // 注册所有工具
            AgentToolManager::instance().registerAllTools();

            // 初始化提示词服务（从数据库加载默认提示词）
            PromptService::instance().initialize();

            m_initialized = true;
        }

        /// @brief 处理消息 - 主流程
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @param message QQ 消息
        /// @return 回复内容（如果需要回复）
        drogon::Task<std::optional<std::string>> process(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory,
            const QQMessage& message
        ){
            if (!m_initialized) {
                spdlog::error("AgentSystem: 未初始化，请先调用 initialize()");
                co_return std::nullopt;
            }

            // 检查是否正在处理该群的消息
            uint64_t groupId = message.getGroupId();
            if (isProcessing(groupId)) {
                spdlog::info("AgentSystem: 群 {} 正在处理中，跳过本次回复（消息已记录）", groupId);
                co_return std::nullopt;
            }

            // 标记为处理中
            markProcessing(groupId);

            // 使用 RAII 确保处理完成后释放锁
            struct ProcessingGuard{
                AgentSystem* sys;
                uint64_t gid;
                ~ProcessingGuard(){ sys->unmarkProcessing(gid); }
            } guard{this, groupId};

            auto& router = RouterAgent::instance();
            auto& planner = PlannerAgent::instance();
            auto& executor = ExecutorAgent::instance();

            spdlog::info("AgentSystem: ========== 开始三层代理流程 ==========");

            // ========== Layer 1: Router Agent ==========
            spdlog::info("AgentSystem: [Layer 1] Router Agent 决策...");
            auto routerDecision = co_await router.route(chatRecords, message);

            spdlog::info("AgentSystem: Router 决策结果 - action={}, reason={}",
                         routerDecision.action, routerDecision.reason);

            // Router SKIP → 直接结束
            if (routerDecision.action == RouterDecision::Action::SKIP) {
                spdlog::info("AgentSystem: Router 决定跳过，结束处理");
                co_return std::nullopt;
            }

            // ========== Layer 2: Planner Agent ==========
            PlanResult plan;

            if (routerDecision.action == RouterDecision::Action::PRIORITY_REPLY) {
                spdlog::info("AgentSystem: [Layer 2] 跳过 Planner（高优先级）");

                // 创建默认计划
                plan.intent = PlanResult::Intent::QUESTION;
                plan.strategy.shouldReply = true;
                plan.strategy.tone = "friendly";
                plan.strategy.maxLength = 100;
                plan.strategy.reason = "高优先级回复（@提及或紧急问题）";
                plan.contextSummary = "需要立即回复";
            } else {
                spdlog::info("AgentSystem: [Layer 2] Planner Agent 规划...");

                if (const auto planResult =
                        co_await planner.plan(chatRecords, memory, routerDecision); !planResult
                ) {
                    spdlog::error("AgentSystem: Planner 规划失败");
                    // 使用默认计划
                    plan.intent = PlanResult::Intent::CHAT;
                    plan.strategy.shouldReply = true;
                    plan.strategy.maxLength = 25;
                    plan.strategy.tone = "friendly";
                    plan.strategy.reason = "Planner失败，使用默认策略";
                } else {
                    plan = planResult.value();
                }

                spdlog::info("AgentSystem: Planner 规划结果 - intent={}, shouldReply={}",
                             plan.intent, plan.strategy.shouldReply);

                // Planner 决定不回复 → 结束
                if (!plan.strategy.shouldReply) {
                    spdlog::info("AgentSystem: Planner 决定不回复，结束处理");
                    co_return std::nullopt;
                }
            }

            // ========== Layer 3: Executor Agent ==========
            spdlog::info("AgentSystem: [Layer 3] Executor Agent 执行...");

            const bool isPriority = routerDecision.action == RouterDecision::Action::PRIORITY_REPLY;
            auto replyDecision
                = co_await executor.execute(chatRecords, memory, plan, isPriority);

            if (!replyDecision) {
                spdlog::error("AgentSystem: Executor 执行失败");
                co_return std::nullopt;
            }

            spdlog::info("AgentSystem: ========== 三层代理流程完成 ==========");

            if (replyDecision->shouldReply && !replyDecision->content.empty()) {
                co_return replyDecision->content;
            }

            co_return std::nullopt;
        }

        /// @brief 处理 @提及消息 - 直接回复路径
        /// @param chatRecords 聊天记录管理器
        /// @param memory 长期记忆管理器
        /// @return 回复内容
        drogon::Task<std::optional<std::string>> processAtMention(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory
        ) const{
            auto& executor = ExecutorAgent::instance();

            spdlog::info("AgentSystem: @提及直接回复模式");

            auto replyDecision = co_await executor.directReply(chatRecords, memory);

            if (replyDecision && replyDecision->shouldReply && !replyDecision->content.empty()) {
                co_return replyDecision->content;
            }

            co_return std::nullopt;
        }

    private:
        AgentSystem() = default;
        bool m_initialized = false;

        // 正在处理中的群聊（防止同时处理多条消息）
        std::unordered_set<uint64_t> m_processingGroups;
        std::mutex m_processingMutex;

        /// @brief 检查群是否正在处理
        bool isProcessing(uint64_t groupId){
            std::lock_guard lock(m_processingMutex);
            return m_processingGroups.contains(groupId);
        }

        /// @brief 标记群为处理中
        void markProcessing(const uint64_t groupId){
            std::lock_guard lock(m_processingMutex);
            m_processingGroups.insert(groupId);
        }

        /// @brief 标记群处理完成
        void unmarkProcessing(const uint64_t groupId){
            std::lock_guard lock(m_processingMutex);
            m_processingGroups.erase(groupId);
        }
    };
} // namespace LittleMeowBot
