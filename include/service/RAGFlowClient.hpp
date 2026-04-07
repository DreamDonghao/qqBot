/// @file RAGFlowClient.hpp
/// @brief RAGFlow 知识库检索客户端
#pragma once
#include <config/Config.hpp>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <string>
#include <optional>

namespace LittleMeowBot {
    class RAGFlowClient{
    public:
        static RAGFlowClient& instance();

        /// @brief 检索知识库
        drogon::Task<std::optional<std::string>> searchKnowledge(
            const std::string& question,
            int topK = 3) const;

        /// @brief 检索记忆库
        drogon::Task<std::optional<std::string>> searchMemory(
            const std::string& question,
            int topK = 3) const;

        /// @brief 添加记忆到记忆库
        drogon::Task<bool> addMemory(const std::string& content) const;

    private:
        RAGFlowClient() = default;

        /// @brief 解析检索结果
        [[nodiscard]] static std::string parseSearchResult(const Json::Value& json);
    };
}