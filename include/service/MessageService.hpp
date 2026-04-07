/// @file MessageService.hpp
/// @brief QQ 消息服务 - 消息发送与处理
#pragma once
#include <config/Config.hpp>
#include <model/ChatRecordManager.hpp>
#include <model/QQMessage.hpp>
#include <service/WebSocketManager.hpp>
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <string>

namespace LittleMeowBot {
    class MessageService{
    public:
        static MessageService& instance();

        /// @brief 将文本中的@格式转换为 CQ 码
        static std::string convertAtToCQCode(const std::string& text);

        /// @brief 发送群消息
        drogon::Task<> sendGroupMsg(
            Json::UInt64 groupId,
            const std::string& message,
            const ChatRecordManager& chatRecords) const;

        /// @brief 禁言群成员
        drogon::Task<bool> setGroupBan(
            Json::UInt64 groupId,
            Json::UInt64 userId,
            Json::UInt64 duration) const;

        /// @brief 获取群信息
        drogon::Task<Json::Value> getGroupInfo(Json::UInt64 groupId) const;

        /// @brief 获取并更新群名称
        drogon::Task<std::string> fetchAndUpdateGroupName(Json::UInt64 groupId) const;

    private:
        MessageService() = default;
    };
}
