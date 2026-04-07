/// @file ApiClient.hpp
/// @brief API 客户端 - LLM API 请求封装
#pragma once
#include <service/ToolRegistry.hpp>
#include <agent/AgentTypes.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <string>
#include <vector>

namespace LittleMeowBot {
    struct ToolCall{
        std::string id;
        std::string name;
        std::string arguments;
    };

    struct ApiResponse{
        std::string content;
        std::vector<ToolCall> toolCalls;
        [[nodiscard]] bool hasToolCalls() const{ return !toolCalls.empty(); }
        [[nodiscard]] bool hasContent() const{ return !content.empty(); }
    };

    class ApiClient{
    public:
        static ApiClient& instance();

        void registerTool(const Tool& tool) const;
        [[nodiscard]] Json::Value getToolsJson() const;
        drogon::Task<std::string> executeTool(const std::string& name, const Json::Value& args) const;

        static drogon::Task<std::string> fetchWeather(const std::string& city);
        static drogon::Task<std::string> searchWeb(const std::string& query);

        drogon::Task<std::optional<std::string>> requestLLM(
            const Json::Value& messages,
            float temperature = 1.35f,
            float top_p = 0.92f,
            int max_tokens = 1024) const;

        drogon::Task<std::optional<ApiResponse>> requestWithTools(
            const Json::Value& messages,
            const Json::Value& tools, const std::string& base_url,
            const std::string& api_key, const std::string& model,
            float temperature = 0.7f,
            float top_p = 0.9f,
            int max_tokens = 1024) const;

        drogon::Task<std::optional<std::string>> requestAgent(
            Json::Value messages,
            const std::string& base_url,
            const std::string& api_key,
            const std::string& model,
            int maxIterations = 5) const;

        drogon::Task<std::optional<ReplyDecision>> requestReplyAgent(
            Json::Value messages,
            const std::string& base_url, const std::string& api_key, const std::string& model,
            int maxIterations = 5) const;

        static drogon::Task<std::optional<std::string>> requestStr(
            const Json::Value& messages,
            const std::string& base_url,
            const std::string& path,
            const std::string& api_key,
            const std::string& model,
            float temperature,
            float top_p,
            int max_tokens);

    private:
        ApiClient() = default;

        static Json::Value buildModelReq(
            const Json::Value& messages,
            const std::string& model,
            float temperature,
            float top_p,
            int max_tokens);
    };
}
