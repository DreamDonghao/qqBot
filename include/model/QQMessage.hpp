/// @file QQMessage.hpp
/// @brief QQ 消息模型
#pragma once

#include <drogon/drogon.h>
#include <unordered_map>
#include <string>

namespace LittleMeowBot {
    class QQMessage{
    public:
        explicit QQMessage(const Json::Value& qqMessageJson);
        void setMessageJson(const Json::Value& qqMessageJson);

        [[nodiscard]] bool atMe() const;
        [[nodiscard]] bool existImage() const;
        [[nodiscard]] Json::UInt64 getGroupId() const;
        [[nodiscard]] Json::UInt64 getSelfQQNumber() const;
        [[nodiscard]] Json::UInt64 getSenderQQNumber() const;
        [[nodiscard]] Json::String getSenderQQName() const;
        [[nodiscard]] Json::String getSenderGroupName() const;
        [[nodiscard]] Json::UInt64 getMessageId() const;

        drogon::Task<> formatMessage();
        [[nodiscard]] Json::String getFormatMessage() const;
        [[nodiscard]] std::string getRawMessage() const;

        static void setCustomQQName(Json::UInt64 qqNumber, const Json::String& qqName);
        static Json::String getQQName(Json::UInt64 qqNumber);
        static std::unordered_map<std::string, uint64_t> getNameToQQMap();
        static void addMessageCache(Json::UInt64 messageId, Json::String message);

    private:
        const Json::Value* m_qqMessageJson;
        Json::String m_formatMessage;
        bool m_isAtMe{false};
        bool m_isExistImage{false};

        inline static std::unordered_map<Json::UInt64, Json::String> m_QQNameMap;
        inline static std::unordered_map<Json::UInt64, Json::String> m_customQQNameMap;
        inline static std::unordered_map<uint64_t, Json::String> m_messageCache;
    };
}
