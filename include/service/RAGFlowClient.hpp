/// @file RAGFlowClient.hpp
/// @brief RAGFlow 知识库检索客户端
/// @author donghao
/// @date 2026-04-02
/// @details 提供 RAGFlow 知识库和记忆库的检索功能：
///          - 知识库检索：searchKnowledge()
///          - 记忆库检索：searchMemory()
///          - 添加记忆：addMemory()

#pragma once
#include <config/Config.hpp>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <string>
#include <optional>

namespace LittleMeowBot {
    /// @brief RAGFlow 知识库检索客户端
    class RAGFlowClient{
    public:
        static RAGFlowClient& instance(){
            static RAGFlowClient client;
            return client;
        }

        /// @brief 检索知识库
        /// @param question 查询问题
        /// @param topK 返回结果数量，默认3
        /// @return 检索结果文本，失败返回 std::nullopt
        drogon::Task<std::optional<std::string>> searchKnowledge(
            const std::string& question,
            const int topK = 3
        ) const{
            const auto& kbConfig = Config::instance().knowledgeBase;
            const auto& apiKey = kbConfig.apiKey;
            const auto& baseUrl = kbConfig.baseUrl;
            const auto& knowledgeDatasetId = kbConfig.knowledgeDatasetId;

            if (knowledgeDatasetId.empty()) {
                spdlog::error("RAGFlow: 知识库ID未配置");
                co_return std::nullopt;
            }

            const auto client = drogon::HttpClient::newHttpClient(baseUrl);

            // 构建请求体
            Json::Value body;
            body["dataset_ids"].append(knowledgeDatasetId);
            body["question"] = question;
            body["top_k"] = topK;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/api/v1/retrieval");
            req->addHeader("Authorization", "Bearer " + apiKey);

            const auto resp = co_await client->sendRequestCoro(req);

            if (resp->getStatusCode() != drogon::k200OK) {
                spdlog::error("RAGFlow 请求失败: status={}", resp->getStatusCode());
                co_return std::nullopt;
            }

            const auto json = resp->getJsonObject();
            if (!json) {
                spdlog::error("RAGFlow 响应解析失败");
                co_return std::nullopt;
            }

            // 解析检索结果
            std::string result = parseSearchResult(*json);
            if (result.empty()) {
                co_return std::nullopt;
            }

            co_return result;
        }

        /// @brief 检索记忆库（预留接口）
        /// @param question 查询问题
        /// @param topK 返回结果数量，默认3
        /// @return 检索结果文本，失败返回 std::nullopt
        drogon::Task<std::optional<std::string>> searchMemory(
            const std::string& question,
            const int topK = 3
        ) const{
            const auto& kbConfig = Config::instance().knowledgeBase;
            const auto& apiKey = kbConfig.apiKey;
            const auto& baseUrl = kbConfig.baseUrl;
            const auto& memoryDatasetId = kbConfig.memoryDatasetId;

            if (memoryDatasetId.empty()) {
                spdlog::warn("RAGFlow: 记忆库ID未配置");
                co_return std::nullopt;
            }

            const auto client = drogon::HttpClient::newHttpClient(baseUrl);

            Json::Value body;
            body["dataset_ids"].append(memoryDatasetId);
            body["question"] = question;
            body["top_k"] = topK;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/api/v1/retrieval");
            req->addHeader("Authorization", "Bearer " + apiKey);

            const auto resp = co_await client->sendRequestCoro(req);

            if (resp->getStatusCode() != drogon::k200OK) {
                spdlog::error("RAGFlow 记忆库请求失败: status={}", resp->getStatusCode());
                co_return std::nullopt;
            }

            const auto json = resp->getJsonObject();
            if (!json) {
                co_return std::nullopt;
            }

            co_return parseSearchResult(*json);
        }

        /// @brief 添加记忆到记忆库
        /// @param content 记忆内容文本
        /// @return 是否成功
        drogon::Task<bool> addMemory(const std::string& content) const{
            const auto& kbConfig = Config::instance().knowledgeBase;
            const auto& apiKey = kbConfig.apiKey;
            const auto& baseUrl = kbConfig.baseUrl;
            const auto& memoryDatasetId = kbConfig.memoryDatasetId;
            const auto& memoryDocumentId = kbConfig.memoryDocumentId;

            if (memoryDatasetId.empty()) {
                spdlog::warn("RAGFlow: 记忆库ID未配置，无法添加记忆");
                co_return false;
            }

            if (memoryDocumentId.empty()) {
                spdlog::warn("RAGFlow: 记忆文档ID未配置，请在管理后台配置");
                co_return false;
            }

            // 添加 chunk 到文档
            const auto client = drogon::HttpClient::newHttpClient(baseUrl);

            Json::Value body;
            body["content"] = content;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath(fmt::format("/api/v1/datasets/{}/documents/{}/chunks",
                                     memoryDatasetId, memoryDocumentId));
            req->addHeader("Authorization", "Bearer " + apiKey);

            if (const auto resp = co_await client->sendRequestCoro(req);
                resp->getStatusCode() != drogon::k200OK) {
                spdlog::error("RAGFlow 添加记忆失败: status={}", resp->getStatusCode());
                co_return false;
            }

            spdlog::info("RAGFlow 记忆已添加: {} 字符", content.size());
            co_return true;
        }

    private:
        RAGFlowClient() = default;

        /// @brief 解析检索结果
        [[nodiscard]] static std::string parseSearchResult(const Json::Value& json){
            if (!json.isMember("data") || !json["data"].isMember("chunks")) {
                spdlog::warn("RAGFlow 返回格式异常");
                return "";
            }

            const auto& chunks = json["data"]["chunks"];
            if (!chunks.isArray() || chunks.empty()) {
                return "未找到相关信息";
            }

            std::string result;
            int count = 0;

            for (const auto& chunk : chunks) {
                if (count >= 3) break;

                if (chunk.isMember("content")) {
                    std::string content = chunk["content"].asString();

                    if (chunk.isMember("similarity")) {
                        if (const float similarity = chunk["similarity"].asFloat(); similarity < 0.3f) continue;
                    }

                    result += content + "\n";
                    count++;
                }
            }

            return result;
        }
    };
}
