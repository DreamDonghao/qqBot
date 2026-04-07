/// @file ToolRegistry.hpp
/// @brief 工具注册中心 - Agent 工具的分类管理与执行
/// @author donghao
/// @date 2026-03-28
/// @details 提供工具的注册、管理和执行功能：
///          - 分类管理：TERMINAL（终端工具）、INFORMATION（信息工具）、ACTION（动作工具）
///          - 工具执行：支持异步执行和上下文传递
///          - 工具定义生成：生成 OpenAI 兼容的工具定义 JSON

#ifndef LITTLE_MEOW_BOT_TOOLREGISTRY_HPP
#define LITTLE_MEOW_BOT_TOOLREGISTRY_HPP

#include <json/value.h>
#include <drogon/utils/coroutine.h>
#include <storage/Database.hpp>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <ranges>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace LittleMeowBot {
    /// @brief 工具执行上下文（线程局部存储）
    struct ToolContext {
        uint64_t groupId = 0;
        std::string groupName;
    };

    /// @brief 获取当前工具执行上下文
    inline ToolContext& currentToolContext() {
        thread_local ToolContext ctx;
        return ctx;
    }

    /// @brief 异步工具处理器
    using ToolHandler = std::function<drogon::Task<std::string>(const Json::Value& args)>;

    struct Tool{
        std::string name;
        std::string description;
        Json::Value parameters; // JSON Schema
        ToolHandler handler;
    };

    /// @brief 工具分类
    enum class ToolCategory{
        TERMINAL, // 终端工具：reply, no_reply（结束处理）
        INFORMATION, // 信息工具：get_weather, search_web（获取数据）
        ACTION // 动作工具：random（执行动作）
    };

    /// @brief 工具注册中心，分类管理工具
    class ToolRegistry{

    public:
        static ToolRegistry& instance(){
            static ToolRegistry registry;
            return registry;
        }

        /// @brief 注册工具到指定分类
        void registerTool(const Tool& tool, ToolCategory category){
            switch (category) {
            case ToolCategory::TERMINAL:
                m_terminalTools[tool.name] = tool;
                break;
            case ToolCategory::INFORMATION:
                m_infoTools[tool.name] = tool;
                break;
            case ToolCategory::ACTION:
                m_actionTools[tool.name] = tool;
                break;
            }
            spdlog::info("工具注册成功: {} (分类: {})",
                         tool.name, categoryToString(category));
        }

        /// @brief 获取指定分类的工具定义（用于 API 请求）
        Json::Value getToolsByCategory(ToolCategory category) const{
            Json::Value tools;

            for (const auto& toolMap = getToolMap(category);
                 const auto& tool : toolMap | std::views::values) {
                Json::Value toolDef;
                toolDef["type"] = "function";
                toolDef["function"]["name"] = tool.name;
                toolDef["function"]["description"] = tool.description;
                toolDef["function"]["parameters"] = tool.parameters;
                tools.append(toolDef);
            }
            return tools;
        }

        /// @brief 获取所有工具定义
        Json::Value getAllTools() const{
            Json::Value tools;

            // 终端工具
            for (const auto& tool : m_terminalTools | std::views::values) {
                Json::Value toolDef;
                toolDef["type"] = "function";
                toolDef["function"]["name"] = tool.name;
                toolDef["function"]["description"] = tool.description;
                toolDef["function"]["parameters"] = tool.parameters;
                tools.append(toolDef);
            }

            // 信息工具
            for (const auto& tool : m_infoTools | std::views::values) {
                Json::Value toolDef;
                toolDef["type"] = "function";
                toolDef["function"]["name"] = tool.name;
                toolDef["function"]["description"] = tool.description;
                toolDef["function"]["parameters"] = tool.parameters;
                tools.append(toolDef);
            }

            // 动作工具
            for (const auto& tool : m_actionTools | std::views::values) {
                Json::Value toolDef;
                toolDef["type"] = "function";
                toolDef["function"]["name"] = tool.name;
                toolDef["function"]["description"] = tool.description;
                toolDef["function"]["parameters"] = tool.parameters;
                tools.append(toolDef);
            }

            return tools;
        }

        /// @brief 获取 Executor 使用的工具（终端 + 信息 + 动作）
        Json::Value getExecutorTools() const{
            return getAllTools();
        }

        /// @brief 获取 Planner 可用的工具（仅信息工具，用于规划）
        Json::Value getPlannerInfoTools() const{
            Json::Value tools;
            for (const auto& tool : m_infoTools | std::views::values) {
                Json::Value toolDef;
                toolDef["type"] = "function";
                toolDef["function"]["name"] = tool.name;
                toolDef["function"]["description"] = tool.description;
                toolDef["function"]["parameters"] = tool.parameters;
                tools.append(toolDef);
            }
            return tools;
        }

        /// @brief 执行工具（异步）
        drogon::Task<std::string> executeTool(const std::string& name, const Json::Value& args, uint64_t groupId = 0) const{
            // 设置上下文
            auto& ctx = currentToolContext();
            ctx.groupId = groupId;
            if (groupId != 0) {
                ctx.groupName = Database::instance().getGroupName(groupId);
            }

            // 查找所有分类
            if (m_terminalTools.contains(name)) {
                co_return co_await m_terminalTools.at(name).handler(args);
            }
            if (m_infoTools.contains(name)) {
                co_return co_await m_infoTools.at(name).handler(args);
            }
            if (m_actionTools.contains(name)) {
                co_return co_await m_actionTools.at(name).handler(args);
            }
            co_return "工具未找到: " + name;
        }

        /// @brief 检查工具是否存在
        bool hasTool(const std::string& name) const{
            return m_terminalTools.contains(name) ||
                m_infoTools.contains(name) ||
                m_actionTools.contains(name);
        }

        /// @brief 检查是否是终端工具
        bool isTerminalTool(const std::string& name) const{
            return m_terminalTools.contains(name);
        }

        /// @brief 检查是否是信息工具
        bool isInfoTool(const std::string& name) const{
            return m_infoTools.contains(name);
        }

        /// @brief 注销工具
        void unregisterTool(const std::string& name){
            m_terminalTools.erase(name);
            m_infoTools.erase(name);
            m_actionTools.erase(name);
            spdlog::info("工具已注销: {}", name);
        }

        /// @brief 清除所有自定义工具（用于重新加载前清理）
        void clearCustomTools(const std::vector<std::string>& customToolNames){
            for (const auto& name : customToolNames) {
                unregisterTool(name);
            }
        }

        /// @brief 获取工具分类
        ToolCategory getCategory(const std::string& name) const{
            if (m_terminalTools.contains(name)) return ToolCategory::TERMINAL;
            if (m_infoTools.contains(name)) return ToolCategory::INFORMATION;
            if (m_actionTools.contains(name)) return ToolCategory::ACTION;
            return ToolCategory::ACTION; // 默认
        }

        /// @brief 获取所有工具名称列表
        std::vector<std::string> getToolNames() const{
            std::vector<std::string> names;

            for (const auto& name : m_terminalTools | std::views::keys) names.push_back(name);
            for (const auto& name : m_infoTools | std::views::keys) names.push_back(name);
            for (const auto& name : m_actionTools | std::views::keys) names.push_back(name);

            return names;
        }

        /// @brief 生成工具说明文本（用于 Executor Prompt）
        std::string getToolsDescription() const{
            std::string desc = "可用工具：\n";

            // 终端工具
            desc += "【终端工具】\n";
            for (const auto& [name, tool] : m_terminalTools) {
                desc += fmt::format("- {}: {}\n", name, tool.description);
            }

            // 信息工具
            desc += "【信息工具】\n";
            for (const auto& [name, tool] : m_infoTools) {
                desc += fmt::format("- {}: {}\n", name, tool.description);
            }

            // 动作工具
            desc += "【动作工具】\n";
            for (const auto& [name, tool] : m_actionTools) {
                desc += fmt::format("- {}: {}\n", name, tool.description);
            }

            return desc;
        }

    private:
        ToolRegistry() = default;

        std::unordered_map<std::string, Tool> m_terminalTools;
        std::unordered_map<std::string, Tool> m_infoTools;
        std::unordered_map<std::string, Tool> m_actionTools;

        const std::unordered_map<std::string, Tool>& getToolMap(ToolCategory category) const{
            switch (category) {
            case ToolCategory::TERMINAL: return m_terminalTools;
            case ToolCategory::INFORMATION: return m_infoTools;
            case ToolCategory::ACTION: return m_actionTools;
            default: return m_actionTools;
            }
        }

        static std::string categoryToString(const ToolCategory category){
            switch (category) {
            case ToolCategory::TERMINAL: return "TERMINAL";
            case ToolCategory::INFORMATION: return "INFORMATION";
            case ToolCategory::ACTION: return "ACTION";
            default: return "UNKNOWN";
            }
        }
    };
} // namespace LittleMeowBot

#endif //LITTLE_MEOW_BOT_TOOLREGISTRY_HPP
