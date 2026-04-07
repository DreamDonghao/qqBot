/// @file ApiClient.hpp
/// @brief API 客户端 - LLM API 请求封装
/// @author donghao
/// @date 2026-04-02
/// @details 封装所有 LLM API 请求功能：
///          - 天气查询：fetchWeather()
///          - 网络搜索：searchWeb()
///          - LLM 请求：requestLLM(), requestAgent()
///          - 工具调用：requestWithTools(), requestReplyAgent()

#pragma once
#include <config/Config.hpp>
#include <service/ToolRegistry.hpp>
#include <agent/AgentTypes.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace LittleMeowBot {
    /// @brief 工具调用结果
    struct ToolCall{
        std::string id; // 工具调用ID
        std::string name; // 工具名
        std::string arguments; // 参数JSON字符串
    };

    /// @brief API响应，包含内容或工具调用
    struct ApiResponse{
        std::string content; // 文本内容
        std::vector<ToolCall> toolCalls; // 工具调用列表
        [[nodiscard]] bool hasToolCalls() const{ return !toolCalls.empty(); }
        [[nodiscard]] bool hasContent() const{ return !content.empty(); }
    };

    /// @brief API 客户端类，封装所有 LLM API 请求
    class ApiClient{
    public:
        static ApiClient& instance(){
            static ApiClient client;
            return client;
        }

        /// @brief 注册工具（兼容旧接口）
        void registerTool(const Tool& tool) const{
            ToolRegistry::instance().registerTool(tool, ToolCategory::ACTION);
        }

        /// @brief 获取工具定义列表（用于API请求）
        [[nodiscard]] Json::Value getToolsJson() const{
            return ToolRegistry::instance().getAllTools();
        }

        /// @brief 执行工具（异步）
        drogon::Task<std::string> executeTool(const std::string& name, const Json::Value& args) const{
            co_return co_await ToolRegistry::instance().executeTool(name, args);
        }

        /// @brief 异步查询天气
        static drogon::Task<std::string> fetchWeather(const std::string& city){
            const auto client = drogon::HttpClient::newHttpClient("https://uapis.cn");
            const auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath("/api/v1/misc/weather?city=" + city);
            req->addHeader("Accept", "application/json");

            const auto resp = co_await client->sendRequestCoro(req);
            if (resp->getStatusCode() != drogon::k200OK) {
                co_return "天气查询失败";
            }

            const auto json = resp->getJsonObject();
            if (!json) {
                co_return "天气数据解析失败";
            }

            // 解析天气数据
            std::string result;
            if (json->isMember("city")) {
                result = (*json)["city"].asString();
            }
            if (json->isMember("weather")) {
                result += " " + (*json)["weather"].asString();
            }
            if (json->isMember("temperature")) {
                result += " " + std::to_string(static_cast<int>((*json)["temperature"].asDouble())) + "°C";
            }
            if (json->isMember("humidity")) {
                result += " 湿度" + std::to_string((*json)["humidity"].asInt()) + "%";
            }
            if (json->isMember("wind_direction") && json->isMember("wind_power")) {
                result += " " + (*json)["wind_direction"].asString() + (*json)["wind_power"].asString();
            }

            co_return result.empty() ? "天气信息获取失败" : result;
        }

        /// @brief 异步网络搜索
        static drogon::Task<std::string> searchWeb(const std::string& query){
            const auto client = drogon::HttpClient::newHttpClient("https://uapis.cn");

            // 构建 JSON 请求体
            Json::Value body;
            body["query"] = query;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/api/v1/search/aggregate");
            req->addHeader("Accept", "application/json");

            try {
                const auto resp = co_await client->sendRequestCoro(req);
                if (resp->getStatusCode() != drogon::k200OK) {
                    co_return "搜索失败";
                }

                const auto json = resp->getJsonObject();
                if (!json) {
                    co_return "搜索结果解析失败";
                }

                // 解析搜索结果
                std::string result;
                if (json->isMember("results") && (*json)["results"].isArray()) {
                    const auto& results = (*json)["results"];
                    int count = 0;
                    for (const auto& item : results) {
                        if (count >= 3) break;
                        if (item.isMember("title")) {
                            result += std::to_string(count + 1) + ". " + item["title"].asString();
                            if (item.isMember("snippet")) {
                                result += "\n   " + item["snippet"].asString();
                            }
                            result += "\n";
                            count++;
                        }
                    }
                }

                co_return result.empty() ? "未找到相关结果" : result;
            }
            catch (const std::exception& e) {
                spdlog::error("搜索异常: {}", e.what());
                co_return "搜索服务暂时不可用";
            }
        }

        /// @brief 请求 LLM API（使用 Executor 配置）
        /// @param messages 消息数组
        /// @param temperature 温度参数
        /// @param top_p 核采样参数
        /// @param max_tokens 最大 token 数
        /// @return 模型回复内容，失败返回 std::nullopt
        drogon::Task<std::optional<std::string>> requestLLM(
            const Json::Value& messages,
            const float temperature = 1.35f,
            const float top_p = 0.92f,
            const int max_tokens = 1024) const{
            const auto& config = Config::instance();
            co_return co_await requestStr(
                messages,
                config.executor.baseUrl,
                config.executor.path,
                config.executor.apiKey,
                config.executor.model,
                temperature,
                top_p,
                max_tokens
            );
        }

        /// @brief 带工具调用的API请求（返回完整响应）
        drogon::Task<std::optional<ApiResponse>> requestWithTools(
            const Json::Value& messages,
            const Json::Value& tools,
            const std::string& base_url,
            const std::string& api_key,
            const std::string& model,
            const float temperature = 0.7f,
            const float top_p = 0.9f,
            const int max_tokens = 1024) const{
            const auto client = drogon::HttpClient::newHttpClient(base_url);
            Json::Value body = buildModelReq(messages, model, temperature, top_p, max_tokens);
            body["tools"] = tools;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/v1/chat/completions");
            req->addHeader("Authorization", "Bearer " + api_key);
            req->addHeader("Content-Type", "application/json");

            const auto resp = co_await client->sendRequestCoro(req);
            const auto json = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                LOG_ERROR << "API请求出错: status=" << resp->getStatusCode();
                co_return std::nullopt;
            }

            const auto& message = (*json)["choices"][0]["message"];
            ApiResponse result;

            // 提取文本内容
            if (message.isMember("content") && !message["content"].isNull()) {
                result.content = message["content"].asString();
            }

            // 提取工具调用
            if (message.isMember("tool_calls")) {
                for (const auto& tc : message["tool_calls"]) {
                    ToolCall call;
                    call.id = tc["id"].asString();
                    call.name = tc["function"]["name"].asString();
                    call.arguments = tc["function"]["arguments"].asString();
                    result.toolCalls.push_back(call);
                }
            }

            co_return result;
        }

        /// @brief Agent循环：自动处理工具调用直到生成最终回复
        drogon::Task<std::optional<std::string>> requestAgent(
            Json::Value messages,
            const std::string& base_url,
            const std::string& api_key,
            const std::string& model,
            int maxIterations = 5) const{
            auto tools = getToolsJson();
            if (tools.empty()) {
                LOG_ERROR << "未注册任何工具";
                co_return std::nullopt;
            }

            for (int i = 0; i < maxIterations; ++i) {
                auto response = co_await requestWithTools(
                    messages, tools, base_url, api_key, model, 0.7f, 0.9f, 1024);

                if (!response) {
                    co_return std::nullopt;
                }

                // 没有工具调用，返回内容
                if (!response->hasToolCalls()) {
                    co_return response->content;
                }

                // 有工具调用，执行并继续对话
                spdlog::info("Agent执行工具调用，轮次{}", i + 1);

                // 添加assistant消息（包含tool_calls）
                Json::Value assistantMsg;
                assistantMsg["role"] = "assistant";
                assistantMsg["content"] = response->hasContent() ? response->content : "";
                Json::Value toolCallsJson;
                for (const auto& tc : response->toolCalls) {
                    Json::Value tcJson;
                    tcJson["id"] = tc.id;
                    tcJson["type"] = "function";
                    tcJson["function"]["name"] = tc.name;
                    tcJson["function"]["arguments"] = tc.arguments;
                    toolCallsJson.append(tcJson);
                }
                assistantMsg["tool_calls"] = toolCallsJson;
                messages.append(assistantMsg);

                // 执行每个工具调用，添加结果
                std::string replyContent;
                bool hasReply = false;
                for (const auto& tc : response->toolCalls) {
                    Json::Value args;
                    Json::Reader reader;
                    reader.parse(tc.arguments, args);

                    // 检查是否是最终决策工具
                    if (tc.name == "no_reply") {
                        co_return std::nullopt;
                    }
                    if (tc.name == "reply") {
                        if (args.isMember("content")) {
                            co_return args["content"].asString();
                        }
                        co_return std::nullopt;
                    }

                    // 其他工具执行并添加结果
                    std::string result;
                    if (tc.name == "get_weather") {
                        result = co_await fetchWeather(args.isMember("city") ? args["city"].asString() : "");
                    }
                    else if (tc.name == "search_web") {
                        result = co_await searchWeb(args.isMember("query") ? args["query"].asString() : "");
                    }
                    else {
                        result = co_await executeTool(tc.name, args);
                    }
                    spdlog::info("工具 {} 返回: {}", tc.name, result);

                    Json::Value toolMsg;
                    toolMsg["role"] = "tool";
                    toolMsg["tool_call_id"] = tc.id;
                    toolMsg["content"] = result;
                    messages.append(toolMsg);
                }
            }

            LOG_ERROR << "Agent达到最大迭代次数";
            co_return std::nullopt;
        }

        /// @brief 专门处理回复决策的Agent（支持多轮工具调用）
        drogon::Task<std::optional<ReplyDecision>> requestReplyAgent(
            Json::Value messages,
            const std::string& base_url,
            const std::string& api_key,
            const std::string& model,
            int maxIterations = 5)
        const{
            auto tools = getToolsJson();
            auto client = drogon::HttpClient::newHttpClient(base_url);

            for (int iter = 0; iter < maxIterations; ++iter) {
                Json::Value body = buildModelReq(messages, model, 0.7f, 0.9f, 512);
                body["tools"] = tools;

                auto req = drogon::HttpRequest::newHttpJsonRequest(body);
                req->setMethod(drogon::Post);
                req->setPath("/v1/chat/completions");
                req->addHeader("Authorization", "Bearer " + api_key);
                req->addHeader("Content-Type", "application/json");

                auto resp = co_await client->sendRequestCoro(req);
                auto json = resp->getJsonObject();

                if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                    LOG_ERROR << "API请求出错: status=" << resp->getStatusCode();
                    co_return std::nullopt;
                }

                const auto& message = (*json)["choices"][0]["message"];
                ReplyDecision decision;

                // 检查工具调用
                if (message.isMember("tool_calls") && !message["tool_calls"].empty()) {
                    bool hasFinalDecision = false;
                    Json::Value assistantMsg;
                    assistantMsg["role"] = "assistant";
                    assistantMsg["content"] = ""; // 必须设置content，否则某些API会报错

                    // 先收集所有工具调用
                    Json::Value toolCallsJson;
                    for (const auto& tc : message["tool_calls"]) {
                        Json::Value tcJson;
                        tcJson["id"] = tc["id"].asString();
                        tcJson["type"] = "function";
                        tcJson["function"]["name"] = tc["function"]["name"].asString();
                        tcJson["function"]["arguments"] = tc["function"]["arguments"].asString();
                        toolCallsJson.append(tcJson);
                    }
                    assistantMsg["tool_calls"] = toolCallsJson;

                    // 处理每个工具调用
                    for (const auto& tc : message["tool_calls"]) {
                        std::string toolName = tc["function"]["name"].asString();
                        std::string toolId = tc["id"].asString();
                        std::string argsStr = tc["function"]["arguments"].asString();

                        spdlog::info("Agent调用工具: {}", toolName);

                        if (toolName == "no_reply") {
                            decision.shouldReply = false;
                            hasFinalDecision = true;
                        }
                        else if (toolName == "reply") {
                            Json::Reader reader;
                            if (Json::Value args;
                                reader.parse(argsStr, args) && args.isMember("content")) {
                                decision.shouldReply = true;
                                decision.content = args["content"].asString();
                                hasFinalDecision = true;
                            }
                        }
                        else {
                            // 其他工具（如get_weather），执行并返回结果
                            Json::Value args;
                            Json::Reader reader;
                            reader.parse(argsStr, args);

                            std::string result;
                            if (toolName == "get_weather") {
                                result = co_await fetchWeather(args.isMember("city") ? args["city"].asString() : "");
                            }
                            else if (toolName == "search_web") {
                                result = co_await searchWeb(args.isMember("query") ? args["query"].asString() : "");
                            }
                            else {
                                result = co_await executeTool(toolName, args);
                            }
                            spdlog::info("工具 {} 返回: {}", toolName, result);

                            // 添加工具结果到消息
                            Json::Value toolMsg;
                            toolMsg["role"] = "tool";
                            toolMsg["tool_call_id"] = toolId;
                            toolMsg["content"] = result;
                            messages.append(toolMsg);
                        }
                    }

                    // 如果有最终决定，返回
                    if (hasFinalDecision) {
                        co_return decision;
                    }

                    // 否则添加assistant消息，继续下一轮
                    messages.append(assistantMsg);
                    continue;
                }

                // 没有工具调用，直接用文本内容
                if (message.isMember("content") && !message["content"].isNull()) {
                    decision.shouldReply = true;
                    decision.content = message["content"].asString();
                    co_return decision;
                }

                // 没有内容也没有工具调用
                decision.shouldReply = false;
                co_return decision;
            }

            LOG_ERROR << "Agent达到最大迭代次数";
            co_return std::nullopt;
        }

        /// @brief 通用 API 请求函数
        static drogon::Task<std::optional<std::string>> requestStr(
            const Json::Value& messages,
            const std::string& base_url,
            const std::string& path,
            const std::string& api_key,
            const std::string& model,
            float temperature,
            float top_p,
            int max_tokens){
            const auto client = drogon::HttpClient::newHttpClient(base_url);
            const Json::Value body = buildModelReq(messages, model, temperature, top_p, max_tokens);
            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath(path);
            req->addHeader("Authorization", "Bearer " + api_key);
            req->addHeader("Content-Type", "application/json");

            const auto resp = co_await client->sendRequestCoro(req);
            const auto json = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !json || !json->isMember("choices")) {
                LOG_ERROR << "API 请求出错: status=" << resp->getStatusCode();
                co_return std::nullopt;
            }

            const auto& choices = (*json)["choices"];
            if (!choices.isArray() || choices.empty()) {
                LOG_ERROR << "API 返回格式错误: choices 不是数组或为空";
                co_return std::nullopt;
            }

            co_return choices[0]["message"]["content"].asString();
        }

    private:
        ApiClient() = default;

        static Json::Value buildModelReq(
            const Json::Value& messages,
            const std::string& model,
            float temperature,
            float top_p,
            int max_tokens){
            Json::Value body;
            body["model"] = model;
            body["temperature"] = temperature;
            body["top_p"] = top_p;
            body["max_tokens"] = max_tokens;
            body["messages"] = messages;
            return body;
        }
    };
} // namespace LittleMeowBot
