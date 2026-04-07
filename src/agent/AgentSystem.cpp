/// @file AgentSystem.cpp
/// @brief Agent 系统 - 实现
/// @author donghao
/// @date 2026-04-02

#include <agent/AgentSystem.hpp>
#include <agent/ExecutorAgent.hpp>
#include <agent/PlannerAgent.hpp>
#include <agent/RouterAgent.hpp>
#include <spdlog/spdlog.h>
#include <service/PromptService.hpp>

namespace LittleMeowBot {
    AgentSystem& AgentSystem::instance(){
        static AgentSystem system;
        return system;
    }

    void AgentSystem::initialize(){
        // 注册所有内置工具
        AgentToolManager::instance().registerAllTools();

        // 注册自定义工具
        AgentToolManager::instance().registerCustomTools();

        // 初始化提示词服务（从数据库加载默认提示词）
        PromptService::instance().initialize();

        m_initialized = true;
    }

    drogon::Task<std::optional<std::string>> AgentSystem::process(
        const ChatRecordManager& chatRecords,
        const MemoryManager& memory,
        const QQMessage& message){
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

        const auto& router = RouterAgent::instance();
        const auto& planner = PlannerAgent::instance();
        const auto& executor = ExecutorAgent::instance();

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
                co_await planner.plan(chatRecords, memory, routerDecision); !planResult) {
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

    drogon::Task<std::optional<std::string>> AgentSystem::processAtMention(
        const ChatRecordManager& chatRecords,
        const MemoryManager& memory) const{
        auto& executor = ExecutorAgent::instance();

        spdlog::info("AgentSystem: @提及直接回复模式");

        if (auto replyDecision =
                co_await executor.directReply(chatRecords, memory);
            replyDecision && replyDecision->shouldReply && !replyDecision->content.empty()) {
            co_return replyDecision->content;
        }

        co_return std::nullopt;
    }

    bool AgentSystem::isProcessing(uint64_t groupId){
        std::lock_guard lock(m_processingMutex);
        return m_processingGroups.contains(groupId);
    }

    void AgentSystem::markProcessing(uint64_t groupId){
        std::lock_guard lock(m_processingMutex);
        m_processingGroups.insert(groupId);
    }

    void AgentSystem::unmarkProcessing(uint64_t groupId){
        std::lock_guard lock(m_processingMutex);
        m_processingGroups.erase(groupId);
    }
} // namespace LittleMeowBot
