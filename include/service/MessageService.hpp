/// @file MessageService.hpp
/// @brief QQ 消息服务 - 消息发送与处理
/// @author donghao
/// @date 2026-04-02
/// @details 封装 QQ 消息的发送和处理逻辑：
///          - 群消息发送：sendGroupMsg()
///          - @格式转换：convertAtToCQCode()
///          - 禁言管理：setGroupBan()
///          - 群信息获取：getGroupInfo()

#pragma once
#include <config/Config.hpp>
#include <model/ChatRecordManager.hpp>
#include <model/QQMessage.hpp>
#include <service/WebSocketManager.hpp>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <regex>

namespace LittleMeowBot {
    /// @brief 消息服务类，封装 QQ 消息发送逻辑
    class MessageService{
    public:
        static MessageService& instance(){
            static MessageService service;
            return service;
        }

        /// @brief 将文本中的@格式转换为 CQ 码
        /// @param text 原始文本
        /// @return 转换后的文本
        static std::string convertAtToCQCode(const std::string& text){
            std::string result = text;

            // 格式 @[...数字...] → 提取数字转为 [CQ:at,qq=数字]
            const std::regex atPattern(R"(@\[.*?(\d{5,11}).*?\])");
            result = std::regex_replace(result, atPattern, "[CQ:at,qq=$1]");

            // 2. 模糊格式 @昵称 → 查找昵称映射
            auto nameToQQ = QQMessage::getNameToQQMap();

            // 按昵称长度降序排序，避免短昵称先匹配
            std::vector<std::pair<std::string, uint64_t>> sortedNames(nameToQQ.begin(), nameToQQ.end());
            std::sort(sortedNames.begin(), sortedNames.end(),
                      [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });

            for (const auto& [name, qq] : sortedNames) {
                std::string atPattern = "@" + name;
                size_t pos = 0;
                while ((pos = result.find(atPattern, pos)) != std::string::npos) {
                    size_t endPos = pos + atPattern.length();
                    bool isComplete = (endPos >= result.length() ||
                        (!std::isalnum(static_cast<unsigned char>(result[endPos])) &&
                            result[endPos] != '_' && result[endPos] != '-'));

                    if (isComplete) {
                        // 检查是否已经是CQ码的一部分（避免重复转换）
                        if (pos >= 4 && result.substr(pos - 4, 4) == "qq=") {
                            pos = endPos;
                            continue;
                        }
                        std::string cqCode = fmt::format("[CQ:at,qq={}]", qq);
                        result.replace(pos, atPattern.length(), cqCode);
                        pos += cqCode.length();
                    } else {
                        pos = endPos;
                    }
                }
            }

            return result;
        }

        /// @brief 发送群消息
        /// @param groupId 群号
        /// @param message 消息内容
        /// @param chatRecords 聊天记录管理器（用于更新记录）
        drogon::Task<> sendGroupMsg(
            Json::UInt64 groupId,
            const std::string& message,
            const ChatRecordManager& chatRecords) const{
            const auto& config = Config::instance();
            const auto client = drogon::HttpClient::newHttpClient(config.qqHttpHost);

            // 转换 @[QQ:xxx] 为 CQ 码
            std::string processedMessage = convertAtToCQCode(message);

            Json::Value body;
            body["group_id"] = groupId;
            body["message"] = processedMessage;
            body["auto_escape"] = false;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/send_group_msg");
            req->addHeader("Authorization", "Bearer " + config.accessToken);

            const auto resp = co_await client->sendRequestCoro(req);
            const auto requestJson = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !requestJson) {
                LOG_ERROR << "发送消息错误: status=" << resp->getStatusCode();
                co_return;
            }

            if (!requestJson->isMember("retcode") || (*requestJson)["retcode"].asInt() < 0) {
                LOG_ERROR << "发送消息错误: retcode=" << requestJson->get("retcode", -1).asInt();
                co_return;
            }

            uint64_t messageId = (*requestJson)["data"]["message_id"].asUInt64();
            const Json::String formatted = Json::String() + "{" + config.botName + "(我)}:" + processedMessage;
            QQMessage::addMessageCache(messageId, formatted);

            // 更新聊天记录
            chatRecords.addAssistantRecord(processedMessage);

            // WebSocket推送
            WebSocketManager::instance().pushMessage(groupId, "assistant", processedMessage);

            spdlog::info("成功发送群消息 {} -> {} (message_id={})", groupId, processedMessage, messageId);
        }

        /// @brief 禁言群成员
        /// @param groupId 群号
        /// @param userId 用户QQ号
        /// @param duration 禁言时长（秒），0表示解除禁言
        /// @return 是否成功
        drogon::Task<bool> setGroupBan(
            Json::UInt64 groupId,
            Json::UInt64 userId,
            Json::UInt64 duration) const{
            const auto& config = Config::instance();
            const auto client = drogon::HttpClient::newHttpClient(config.qqHttpHost);

            Json::Value body;
            body["group_id"] = groupId;
            body["user_id"] = userId;
            body["duration"] = duration;

            const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
            req->setMethod(drogon::Post);
            req->setPath("/set_group_ban");
            req->addHeader("Authorization", "Bearer " + config.accessToken);

            const auto resp = co_await client->sendRequestCoro(req);
            const auto requestJson = resp->getJsonObject();

            if (resp->getStatusCode() != drogon::k200OK || !requestJson) {
                spdlog::error("禁言请求失败: status={}", resp->getStatusCode());
                co_return false;
            }

            int retcode = requestJson->get("retcode", -1).asInt();
            if (retcode != 0) {
                spdlog::error("禁言失败: retcode={}", retcode);
                co_return false;
            }

            spdlog::info("禁言成功: 群{} 用户{} 时长{}秒", groupId, userId, duration);
            co_return true;
        }

        /// @brief 获取群信息
        /// @param groupId 群号
        /// @return 群信息JSON（包含group_name等）
        drogon::Task<Json::Value> getGroupInfo(Json::UInt64 groupId) const{
            const auto& config = Config::instance();
            const auto client = drogon::HttpClient::newHttpClient(config.qqHttpHost);

            const auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath(fmt::format("/get_group_info?group_id={}", groupId));
            req->addHeader("Authorization", "Bearer " + config.accessToken);

            const auto resp = co_await client->sendRequestCoro(req);

            Json::Value result;
            if (resp->getStatusCode() == drogon::k200OK) {
                result = *resp->getJsonObject();
            }

            co_return result;
        }

        /// @brief 获取并更新群名称
        /// @param groupId 群号
        /// @return 群名称
        drogon::Task<std::string> fetchAndUpdateGroupName(Json::UInt64 groupId) const{
            auto result = co_await getGroupInfo(groupId);

            std::string groupName;
            if (result.isMember("data") && result["data"].isMember("group_name")) {
                groupName = result["data"]["group_name"].asString();
                Database::instance().updateGroupName(groupId, groupName);
            }

            co_return groupName;
        }

    private:
        MessageService() = default;
    };
} // namespace LittleMeowBot