#pragma once
#include "Config.hpp"
#include "BotMemory.hpp"
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>

namespace qqBot {
    /// @brief 消息服务类，封装 QQ 消息发送逻辑
    class MessageService{
    public:
        static MessageService& instance(){
            static MessageService service;
            return service;
        }

        /// @brief 发送群消息
        /// @param groupId 群号
        /// @param message 消息内容
        /// @param botMemories Bot 记忆引用（用于更新记忆）
        drogon::Task<> sendGroupMsg(
            Json::UInt64 groupId,
            const std::string& message,
            std::unordered_map<Json::UInt64, BotMemory>& botMemories) const{
            const auto& config = Config::instance();
            const auto client = drogon::HttpClient::newHttpClient(config.qq_http_host);

            Json::Value body;
            body["group_id"] = groupId;
            body["message"] = message;
            body["auto_escape"] = false;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/send_group_msg");
            req->addHeader("Authorization", "Bearer " + config.access_token);

            const auto resp = co_await client->sendRequestCoro(req);
            const auto requestJson = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !requestJson) {
                LOG_ERROR << "发送消息错误: status=" << resp->getStatusCode();
                co_return;
            }

            if (!requestJson->isMember("retcode") || (*requestJson)["retcode"].asInt() != 0) {
                LOG_ERROR << "发送消息错误: retcode=" << (*requestJson).get("retcode", -1).asInt();
                co_return;
            }

            uint64_t messageId = (*requestJson)["data"]["message_id"].asUInt64();
            const Json::String formatted = Json::String() + "{" + "小喵(我)" + "}:" + message;
            QQMessage::addMessageCache(messageId, formatted);
            botMemories[groupId].addAssistantChatRecord(message);

            spdlog::info("成功发送群消息 {} -> {} (message_id={})", groupId, message, messageId);
        }

    private:
        MessageService() = default;
    };
} // namespace qqBot
