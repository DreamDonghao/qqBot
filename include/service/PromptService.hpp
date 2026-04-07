/// @file PromptService.hpp
/// @brief 提示词服务类
/// @details 从数据库加载和管理提示词，支持运行时动态修改
#pragma once
#include <string>

namespace LittleMeowBot {
    class PromptService{
    public:
        static PromptService& instance();

        /// @brief 初始化提示词（如果不存在则插入默认值）
        void initialize() const;

        /// @brief 获取提示词（支持占位符替换）
        std::string getPrompt(const std::string& key) const;

        /// @brief 设置提示词（运行时修改）
        void setPrompt(const std::string& key, const std::string& content) const;

        /// @brief 获取 Executor 系统提示词
        std::string getExecutorSystemPrompt() const;

        /// @brief 获取 Executor 提醒提示词
        std::string getExecutorRemindPrompt() const;

    private:
        PromptService() = default;
    };
}