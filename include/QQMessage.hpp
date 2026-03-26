//
// Created by donghao on 2026/1/19.
//
#ifndef QQ_BOT_QQMESSAGE_HPP
#define QQ_BOT_QQMESSAGE_HPP
#include <drogon/drogon.h>
#include <fstream>
#include <shared_mutex>
#include <tool.h>
#include <utility>
inline std::unordered_map<std::string, std::shared_mutex> fileLocks;

inline std::shared_mutex &getFileMutex(const std::string &path) {
    static std::mutex mapMutex;
    std::lock_guard lock(mapMutex);
    return fileLocks[path];
}


inline Json::Value loadJson(const std::string &path) {
    auto &m = getFileMutex(path);
    std::shared_lock lock(m); // 共享锁
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("cannot open file");
    }
    Json::Value root;
    std::string errs;
    if (const Json::CharReaderBuilder builder; !Json::parseFromStream(builder, ifs, &root, &errs)) {
        throw std::runtime_error("json parse error: " + errs);
    }
    return root;
}


inline void writeJsonAtomic(const std::string &path, const Json::Value &root) {
    auto &m = getFileMutex(path);
    std::lock_guard lock(m);

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw std::runtime_error("cannot open temp file");
    }

    Json::StreamWriterBuilder builder;
    builder["emitUTF8"] = true;
    builder["indentation"] = "  ";

    const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &ofs);
    ofs.flush();
}


namespace qqBot {

    inline const std::string chat_api_key = "";
    inline const std::string chat_api_base_url = "";
    inline const std::string chat_api_model = "";

    inline drogon::Task<std::string> getImageDescribe(const std::string &imageUrl) {
        auto client = drogon::HttpClient::newHttpClient(chat_api_base_url);
        Json::Value body;
        body["model"] = chat_api_model;
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
        req->setPath("/compatible-mode/v1/chat/completions");
        req->addHeader("Authorization", "Bearer " + chat_api_key);

        auto resp = co_await client->sendRequestCoro(req);
        const auto json = resp->getJsonObject();
        const auto &choices = (*json)["choices"];
        const auto &content = choices[0]["message"]["content"].asString();

        co_return content;
    }


    inline Json::String getQQName(Json::UInt64) {
        static std::unordered_map<Json::UInt64, Json::String> qqMap;
        return "test";
    }

    class QQMessage {
    public:
        explicit QQMessage(const Json::Value &qqMessageJson) : m_qqMessageJson(&qqMessageJson) {
            if (const Json::UInt64 qqNumber = getSenderQQNumber(); m_customQQNameMap.contains(qqNumber)) {
                m_QQNameMap[qqNumber] = m_customQQNameMap[qqNumber];
            } else {
                m_QQNameMap[qqNumber] = getSenderQQName();
            }
            for (const auto &item: (*m_qqMessageJson)["message"]) {
                if (item["type"] == "at") {
                    if (std::stoul(item["data"]["qq"].asString()) == getSelfQQNumber()) {
                        isAtMe = true;
                    }
                } else if (item["type"] == "image") {
                    isExistImage = true;
                }
            }
        }

        void setMessageJson(const Json::Value &qqMessageJson) {
            m_qqMessageJson = &qqMessageJson;
            if (const Json::UInt64 qqNumber = getSenderQQNumber(); m_customQQNameMap.contains(qqNumber)) {
                m_QQNameMap[qqNumber] = m_customQQNameMap[qqNumber];
            } else {
                m_QQNameMap[qqNumber] = getSenderQQName();
            }
            for (const auto &item: (*m_qqMessageJson)["message"]) {
                if (item["type"] == "at") {
                    if (std::stoul(item["data"]["qq"].asString()) == getSelfQQNumber()) {
                        isAtMe = true;
                    }
                } else if (item["type"] == "image") {
                    isExistImage = true;
                }
            }
        }

        [[nodiscard]] bool atMe() const { return isAtMe; }

        [[nodiscard]] bool existImage() const { return isExistImage; }

        [[nodiscard]] Json::UInt64 getGroupId() const { return (*m_qqMessageJson)["group_id"].asUInt64(); }

        /// @brief 获取自己 QQ 号
        [[nodiscard]] Json::UInt64 getSelfQQNumber() const { return (*m_qqMessageJson)["self_id"].asUInt64(); }

        /// @brief 获取发送者 QQ 号
        [[nodiscard]] Json::UInt64 getSenderQQNumber() const {
            return (*m_qqMessageJson)["sender"]["user_id"].asUInt64();
        }

        /// @brief 获取发送者 QQ 昵称
        [[nodiscard]] Json::String getSenderQQName() const {
            return (*m_qqMessageJson)["sender"]["nickname"].asString();
        }

        /// @brief 获取发送者群昵称
        [[nodiscard]] Json::String getSenderGroupName() const {
            return (*m_qqMessageJson)["sender"]["card"].asString();
        }

        [[nodiscard]] Json::UInt64 getMessageId() const { return (*m_qqMessageJson)["message_id"].asUInt64(); }

        drogon::Task<> formatMessage() {
            m_formatMessage.append("[当前日期:"+ currentDateTime()+"]");
            m_formatMessage.append("{" + getQQName(getSenderQQNumber()) + "}:");
            for (const auto &item: (*m_qqMessageJson)["message"]) {
                if (item["type"] == "text") {
                    m_formatMessage.append(item["data"]["text"].asString());
                } else if (item["type"] == "at") {
                    m_formatMessage.append("@{" + getQQName(std::stoul(item["data"]["qq"].asString())) + "}");
                } else if (item["type"] == "face") {
                    m_formatMessage.append(item["data"]["raw"]["faceText"].asString());
                } else if (item["type"] == "image") {
                    m_formatMessage.append("[图片：" +co_await getImageDescribe(item["data"]["url"].asString())+  "]");
                } else if (item["type"] == "reply") {
                    uint64_t replyId = std::stoull(item["data"]["id"].asString());
                    if (auto it = m_messageCache.find(replyId); it != m_messageCache.end()) {
                        m_formatMessage.append("[回复 ");
                        m_formatMessage.append(it->second); // 已格式化文本
                        m_formatMessage.append("]");
                    } else {
                        m_formatMessage.append("[回复消息 ");
                        m_formatMessage.append(item["data"]["id"].asString());
                        m_formatMessage.append("]");
                    }
                }
            }
            m_messageCache[getMessageId()] = m_formatMessage;
            co_return;
        }

        [[nodiscard]] Json::String getFormatMessage() const { return m_formatMessage; }

        [[nodiscard]] std::string getRawMessage() const{
            return (*m_qqMessageJson)["raw_message"].asString();
        }

        static void setCustomQQName(const Json::UInt64 qqNumber, Json::String qqName) {
            m_customQQNameMap[qqNumber] = qqName;
            m_QQNameMap[qqNumber] = std::move(qqName);
        }

        static void loadQQNameMap(const std::string &path) {
            for (const auto &item: loadJson(path)) {
                m_QQNameMap[item["QQNumber"].asUInt64()] = item["QQName"].asString();
            }
        }

        static void saveQQNameMap(const std::string &path) {
            Json::Value data;
            for (const auto &[QQNumber, QQName]: m_QQNameMap) {
                Json::Value item;
                item["QQNumber"] = QQNumber;
                item["QQName"] = QQName;
                data.append(item);
            }
            writeJsonAtomic(path, data);
        }

        static Json::String getQQName(const Json::UInt64 qqNumber) {
            if (m_QQNameMap.contains(qqNumber)) {
                return m_QQNameMap[qqNumber];
            }
            return "未知";
        }

        static void addMessageCache(const Json::UInt64 messageId, Json::String message) {
            std::unique_lock lock(m);
            m_messageCache[messageId] = std::move(message);
        }

    private:
        const Json::Value *m_qqMessageJson;
        Json::String m_formatMessage;
        bool isAtMe = false;
        bool isExistImage = false;
        inline static std::unordered_map<Json::UInt64, Json::String> m_QQNameMap;
        inline static std::unordered_map<Json::UInt64, Json::String> m_customQQNameMap;
        inline static std::shared_mutex m;
        inline static std::unordered_map<uint64_t, Json::String> m_messageCache;
    };
} // namespace qqBot

#endif // QQ_BOT_QQMESSAGE_HPP
