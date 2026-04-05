/// @file QQMessage.hpp
/// @brief QQ 消息模型
/// @author donghao
/// @date 2026-04-02
/// @details 解析和处理 QQ 群消息，支持：
///          - 消息格式化（日期、用户名、@提及、图片识别）
///          - CQ 码解析（文本、表情、图片、回复引用）
///          - 用户昵称管理（自定义昵称映射）

#pragma once
#include <config/Config.hpp>
#include <storage/Database.hpp>
#include <drogon/drogon.h>
#include <fstream>
#include <util/tool.h>
#include <utility>

namespace LittleMeowBot {
    inline drogon::Task<std::string> getImageDescribe(const std::string& imageUrl){
        const auto& config = Config::instance();
        const auto client = drogon::HttpClient::newHttpClient(config.image.baseUrl);
        Json::Value body;
        body["model"] = config.image.model;
        body["messages"] = parseJson(fmt::format(
            R"([{{"role":"user","content":[
                        {{"type":"image_url","image_url":{{"url":"{}"}}}},
                        {{"type":"text","text":"用不到150字描述这张图片"}}
        ]}}])",
            imageUrl));
        body["temperature"] = 0.7;
        body["max_tokens"] = 300;
        body["top_p"] = 0.92;

        const auto req = drogon::HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath(config.image.path);
        req->addHeader("Authorization", "Bearer " + config.image.apiKey);

        const auto resp = co_await client->sendRequestCoro(req);
        if (resp->getStatusCode() != drogon::k200OK) {
            spdlog::error("图像描述请求失败: status={}", resp->getStatusCode());
            co_return "无法识别图片";
        }
        const auto json = resp->getJsonObject();
        if (!json || !json->isMember("choices")) {
            spdlog::error("图像描述响应格式错误");
            co_return "图片识别失败";
        }
        const auto& choices = (*json)["choices"];
        const auto& content = choices[0]["message"]["content"].asString();

        co_return content;
    }

    class QQMessage{
    public:
        explicit QQMessage(const Json::Value& qqMessageJson){
            setMessageJson(qqMessageJson);
        }

        void setMessageJson(const Json::Value& qqMessageJson){
            m_qqMessageJson = &qqMessageJson;
            const Json::UInt64 qqNumber = getSenderQQNumber();
            const Json::UInt64 selfQQ = getSelfQQNumber();
            if (m_customQQNameMap.contains(qqNumber)) {
                m_QQNameMap[qqNumber] = m_customQQNameMap[qqNumber];
            } else {
                std::string name = getSenderQQName();
                // 如果昵称包含 Bot 名称且不是机器人自己，加标注
                const std::string& botName = Config::instance().botName;
                if (name.find(botName) != std::string::npos && qqNumber != selfQQ) {
                    name += "(昵称也为" + botName + "，但不是我)";
                }
                m_QQNameMap[qqNumber] = name;
            }
            for (const auto& item : (*m_qqMessageJson)["message"]) {
                if (item["type"] == "at") {
                    if (std::stoul(item["data"]["qq"].asString()) == getSelfQQNumber()) {
                        m_isAtMe = true;
                    }
                } else if (item["type"] == "image") {
                    m_isExistImage = true;
                }
            }
        }

        [[nodiscard]] bool atMe() const{
            return m_isAtMe;
        }

        [[nodiscard]] bool existImage() const{
            return m_isExistImage;
        }

        [[nodiscard]] Json::UInt64 getGroupId() const{ return (*m_qqMessageJson)["group_id"].asUInt64(); }

        /// @brief 获取自己 QQ 号
        [[nodiscard]] Json::UInt64 getSelfQQNumber() const{ return (*m_qqMessageJson)["self_id"].asUInt64(); }

        /// @brief 获取发送者 QQ 号
        [[nodiscard]] Json::UInt64 getSenderQQNumber() const{
            return (*m_qqMessageJson)["sender"]["user_id"].asUInt64();
        }

        /// @brief 获取发送者 QQ 昵称
        [[nodiscard]] Json::String getSenderQQName() const{
            return (*m_qqMessageJson)["sender"]["nickname"].asString();
        }

        /// @brief 获取发送者群昵称
        [[nodiscard]] Json::String getSenderGroupName() const{
            return (*m_qqMessageJson)["sender"]["card"].asString();
        }

        [[nodiscard]] Json::UInt64 getMessageId() const{ return (*m_qqMessageJson)["message_id"].asUInt64(); }

        drogon::Task<> formatMessage(){
            m_formatMessage.append("[当前日期:" + currentDateTime() + "]");
            const uint64_t senderQQ = getSenderQQNumber();
            m_formatMessage.append("{" + getQQName(senderQQ) + "[QQ:" + std::to_string(senderQQ) + "]}:");
            for (const auto& item : (*m_qqMessageJson)["message"]) {
                if (item["type"] == "text") {
                    m_formatMessage.append(item["data"]["text"].asString());
                } else if (item["type"] == "at") {
                    const uint64_t atQQ = std::stoul(item["data"]["qq"].asString());
                    m_formatMessage.append("@[" + getQQName(atQQ) + ":" + std::to_string(atQQ) + "]");
                } else if (item["type"] == "face") {
                    m_formatMessage.append(item["data"]["raw"]["faceText"].asString());
                } else if (item["type"] == "image") {
                    m_formatMessage.append("[图片：" + co_await getImageDescribe(item["data"]["url"].asString()) + "]");
                } else if (item["type"] == "reply") {
                    uint64_t replyId = std::stoull(item["data"]["id"].asString());

                    // 优先从内存缓存获取
                    std::string cachedText;
                    if (auto it = m_messageCache.find(replyId); it != m_messageCache.end()) {
                        cachedText = it->second;
                    } else {
                        // 尝试从数据库获取
                        if (auto dbCached = Database::instance().getCachedMessage(replyId)) {
                            cachedText = *dbCached;
                            m_messageCache[replyId] = cachedText; // 更新内存缓存
                        }
                    }

                    if (!cachedText.empty()) {
                        m_formatMessage.append("[回复 ");
                        m_formatMessage.append(cachedText);
                        m_formatMessage.append("]");
                    } else {
                        m_formatMessage.append("[回复消息 ");
                        m_formatMessage.append(item["data"]["id"].asString());
                        m_formatMessage.append("]");
                    }
                }
            }
            // 保存到缓存
            m_messageCache[getMessageId()] = m_formatMessage;
            Database::instance().cacheMessage(getMessageId(), m_formatMessage);
            co_return;
        }

        [[nodiscard]] Json::String getFormatMessage() const{ return m_formatMessage; }

        [[nodiscard]] std::string getRawMessage() const{
            return (*m_qqMessageJson)["raw_message"].asString();
        }

        /// @brief 设置自定义 QQ 昵称
        static void setCustomQQName(const Json::UInt64 qqNumber, const Json::String& qqName){
            m_customQQNameMap[qqNumber] = qqName;
            m_QQNameMap[qqNumber] = qqName;
        }

        /// @brief 获取 QQ 昵称
        static Json::String getQQName(const Json::UInt64 qqNumber){
            if (m_QQNameMap.contains(qqNumber)) {
                return m_QQNameMap[qqNumber];
            }
            return "未知";
        }

        /// @brief 获取昵称到QQ号的反向映射（用于@转换）
        static std::unordered_map<std::string, uint64_t> getNameToQQMap(){
            std::unordered_map<std::string, uint64_t> nameToQQ;
            for (const auto& [qq, name] : m_QQNameMap) {
                nameToQQ[name] = qq;
            }
            return nameToQQ;
        }

        /// @brief 添加消息缓存
        static void addMessageCache(const Json::UInt64 messageId, Json::String message){
            // 限制缓存大小，保留最近100条
            if (constexpr size_t MAX_CACHE_SIZE = 100;
                m_messageCache.size() >= MAX_CACHE_SIZE) {
                // 简单策略：清除一半旧缓存
                size_t toRemove = MAX_CACHE_SIZE / 2;
                auto it = m_messageCache.begin();
                while (toRemove-- > 0 && it != m_messageCache.end()) {
                    it = m_messageCache.erase(it);
                }
            }
            m_messageCache[messageId] = std::move(message);
            // 同时保存到数据库
            Database::instance().cacheMessage(messageId, m_messageCache[messageId]);
        }

    private:
        const Json::Value* m_qqMessageJson;       ///< QQ 消息 JSON 指针
        Json::String m_formatMessage;             ///< 格式化后的消息
        bool m_isAtMe{false};                     ///< 是否 @ 了机器人
        bool m_isExistImage{false};               ///< 是否包含图片


        inline static std::unordered_map<Json::UInt64, Json::String> m_QQNameMap;         ///< QQ 号到昵称映射
        inline static std::unordered_map<Json::UInt64, Json::String> m_customQQNameMap;   ///< 自定义昵称映射
        inline static std::unordered_map<uint64_t, Json::String> m_messageCache;          ///< 消息缓存
    };
}
