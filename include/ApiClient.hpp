//
// Created by donghao on 2026/3/18.
//
#ifndef QQ_BOT_APICLIENT_HPP
#define QQ_BOT_APICLIENT_HPP

#include "Config.hpp"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace qqBot {

/// @brief API 客户端类，封装所有 LLM API 请求
class ApiClient {
public:
    static ApiClient& instance() {
        static ApiClient client;
        return client;
    }

    /// @brief 请求 DeepSeek API
    /// @param messages 消息数组
    /// @param temperature 温度参数
    /// @param top_p 核采样参数
    /// @param max_tokens 最大 token 数
    /// @return 模型回复内容，失败返回 std::nullopt
    drogon::Task<std::optional<std::string>> requestDeepSeek(
        const Json::Value& messages,
        float temperature = 1.35f,
        float top_p = 0.92f,
        int max_tokens = 1024) const{

        const auto& config = Config::instance();
        co_return co_await requestStr(
            messages,
            config.ds_api_base_url,
            "/v1/chat/completions",
            config.ds_api_key,
            config.ds_api_model,
            temperature,
            top_p,
            max_tokens
        );
    }

    /// @brief 请求 Qwen API
    drogon::Task<std::optional<std::string>> requestQwen(
        const Json::Value& messages,
        float temperature = 1.35f,
        float top_p = 0.92f,
        int max_tokens = 1024) const{

        auto& config = Config::instance();
        co_return co_await requestStr(
            messages,
            config.qwen_api_base_url,
            "/v1/chat/completions",
            config.qwen_api_key,
            config.qwen_api_model,
            temperature,
            top_p,
            max_tokens
        );
    }

    /// @brief 请求 Qwen API
    drogon::Task<std::optional<std::string>> requestPlan(
        const Json::Value& messages,
        const float temperature = 1.35f,
        const float top_p = 0.92f,
        const int max_tokens = 1024) const{

        auto& config = Config::instance();
        co_return co_await requestStr(
            messages,
            config.plan_api_base_url,
            "/v1/chat/completions",
            config.plan_api_key,
            config.plan_api_model,
            temperature,
            top_p,
            max_tokens
        );
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
        int max_tokens) {

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
        int max_tokens) {

        Json::Value body;
        body["model"] = model;
        body["temperature"] = temperature;
        body["top_p"] = top_p;
        body["max_tokens"] = max_tokens;
        body["messages"] = messages;
        return body;
    }
};

} // namespace qqBot

#endif //QQ_BOT_APICLIENT_HPP