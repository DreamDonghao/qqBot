/// @file OneBotMessage.cpp
/// @brief OneBot 消息模型 - 实现

#include <algorithm>
#include <config/Config.hpp>
#include <model/OneBotMessage.hpp>
#include <service/LlmClient.hpp>
#include <util/HttpUtil.hpp>
#include <util/Logger.hpp>
#include <utility>

namespace insoulforge {
    namespace {
        /// @brief 调用视觉模型描述一张图片
        /// @return 成功时返回描述；请求或响应无效时返回空值
        drogon::Task<std::optional<std::string>> describeImage(std::string imageUrl, const uint64_t sessionId) {
            const auto &config = Config::instance();
            // 图像描述请求不发送 reasoning_effort（图像模型不支持，保持既有行为）
            LLMApiConfig api = config.image;
            api.reasoningEffort.clear();
            constexpr LLMModelParams params{.maxTokens = 300, .temperature = 0.7, .topP = 0.92};
            const json body = LlmClient::buildChatRequestBody(api, params,
              parseJson(fmt::format(
                R"([{{"role":"user","content":[
                    {{"type":"image_url","image_url":{{"url":"{}"}}}},
                    {{"type":"text","text":"用不到150字描述这张图片"}}
            ]}}])",
                imageUrl)));

            const auto resp = co_await HttpUtil::send("[Image]", config.image.baseUrl, config.image.path, drogon::Post,
              body, config.image.apiKey, 90.0, sessionId);
            if (!resp) {
                co_return std::nullopt;
            }

            const auto respJson = LlmClient::validChatJson(*resp);
            if (!respJson) {
                if ((*resp)->getStatusCode() != drogon::k200OK) {
                    Logger::session(sessionId).error(
                      "[Image] 图像描述请求失败: status={}", static_cast<int>((*resp)->getStatusCode()));
                    co_return std::nullopt;
                }
                Logger::session(sessionId).error("[Image] 图像描述响应格式错误");
                co_return std::nullopt;
            }

            LlmClient::logUsage(*respJson, config.image.model, "image", sessionId);

            const json &message = atOrNull((*respJson)["choices"][0], "message");
            const std::string description = jsonToString(atOrNull(message, "content"));
            co_return description.empty() ? std::nullopt : std::optional<std::string>{description};
        }
    } // namespace

    OneBotMessage::OneBotMessage(json event) : m_event(std::move(event)) {
        const uint64_t qqNumber = getSenderQQNumber();
        const uint64_t selfQQ = getSelfQQNumber();
        if (m_customQQNameMap.contains(qqNumber)) {
            m_QQNameMap[qqNumber] = m_customQQNameMap[qqNumber];
        } else {
            std::string name = getSenderQQName();
            if (const std::string &botName = Config::instance().botName;
              name.find(botName) != std::string::npos && qqNumber != selfQQ) {
                name += "(昵称也为" + botName + "，但不是我)";
            }
            m_QQNameMap[qqNumber] = name;
        }
        for (const auto &item: atOrNull(m_event, "message")) {
            if (atOrNull(item, "type") == "at") {
                if (parseUInt64(jsonToString(atOrNull(atOrNull(item, "data"), "qq"))) == getSelfQQNumber()) {
                    m_isAtMe = true;
                }
            } else if (atOrNull(item, "type") == "reply") {
                m_replyTo = parseUInt64(jsonToString(atOrNull(atOrNull(item, "data"), "id")));
            }
        }
    }

    bool OneBotMessage::atMe() const { return m_isAtMe; }

    bool OneBotMessage::hasFace() const { return hasSegment("face"); }

    bool OneBotMessage::isPokeForBot() const {
        for (const auto &item: atOrNull(m_event, "message")) {
            if (getStr(item, "type") == "poke" &&
                getUInt(atOrNull(item, "data"), "target_id", 0) == getSelfQQNumber()) {
                return true;
            }
        }
        return false;
    }

    bool OneBotMessage::isPassivePoke() const { return hasSegment("poke") && !isPokeForBot(); }

    bool OneBotMessage::hasMembershipNotification() const {
        for (const auto &item: atOrNull(m_event, "message")) {
            if (getStr(item, "type") != "notification") {
                continue;
            }
            const std::string kind = getStr(atOrNull(item, "data"), "kind");
            if (kind == "member_join" || kind == "member_leave") {
                return true;
            }
        }
        return false;
    }

    bool OneBotMessage::isPriorityMessage() const {
        return m_isAtMe || isPrivate() || getSenderQQNumber() == kSystemAccountId;
    }

    uint64_t OneBotMessage::getGroupId() const { return jsonToUInt64(atOrNull(m_event, "group_id")); }

    bool OneBotMessage::isPrivate() const { return jsonToString(atOrNull(m_event, "message_type")) == "private"; }

    uint64_t OneBotMessage::getSessionId() const {
        if (isPrivate())
            return getUserId() | kPrivateSessionFlag;
        return getGroupId();
    }

    uint64_t OneBotMessage::getUserId() const {
        return jsonToUInt64(atOrNull(m_event, "user_id"), getSenderQQNumber());
    }

    uint64_t OneBotMessage::getSelfQQNumber() const { return jsonToUInt64(atOrNull(m_event, "self_id")); }

    uint64_t OneBotMessage::getSenderQQNumber() const {
        return jsonToUInt64(atOrNull(atOrNull(m_event, "sender"), "user_id"));
    }

    std::string OneBotMessage::getSenderQQName() const {
        return jsonToString(atOrNull(atOrNull(m_event, "sender"), "nickname"));
    }

    uint64_t OneBotMessage::getMessageId() const { return jsonToUInt64(atOrNull(m_event, "message_id")); }

    drogon::Task<> OneBotMessage::enrichContent() {
        const uint64_t senderQQ = getSenderQQNumber();
        const uint64_t msgId = getMessageId();
        const auto senderName = std::string(getQQName(senderQQ));
        const std::string timeStr = currentDateTime();

        std::string textContent;
        json images = json::array();
        json faces = json::array();
        json notifications = json::array();
        json segments = json::array();
        for (const auto &item: atOrNull(m_event, "message")) {
            const std::string type = getStr(item, "type");
            const json &data = atOrNull(item, "data");
            if (type == "text") {
                const std::string text = getStr(data, "text");
                textContent += text;
                segments.push_back({{"type", "text"}, {"text", text}});
            } else if (type == "at") {
                const uint64_t atQQ = parseUInt64(jsonToString(atOrNull(atOrNull(item, "data"), "qq")));
                segments.push_back(
                  {{"type", "at"}, {"qq", std::to_string(atQQ)}, {"name", std::string(getQQName(atQQ))}});
            } else if (type == "face") {
                json face;
                face["id"] = getStr(data, "id");
                face["label"] = getStr(atOrNull(data, "raw"), "faceText");
                faces.push_back(face);
                face["type"] = "face";
                segments.push_back(std::move(face));
            } else if (type == "image") {
                json image;
                image["file"] = getStr(data, "file");
                image["url"] = getStr(data, "url");
                if (const auto description = co_await describeImage(getStr(data, "url"), getSessionId())) {
                    image["recognition_status"] = "succeeded";
                    image["description"] = *description;
                } else {
                    image["recognition_status"] = "failed";
                }
                images.push_back(image);
                image["type"] = "image";
                segments.push_back(std::move(image));
            } else if (type == "poke" || type == "notification") {
                json notification = data;
                notification["type"] = type;
                notifications.push_back(notification);
                segments.push_back(std::move(notification));
            }
        }

        json msgJson;
        msgJson["time"] = timeStr;
        msgJson["sender"]["name"] = senderName;
        msgJson["sender"]["qq"] = std::to_string(senderQQ);
        msgJson["message_id"] = std::to_string(msgId);
        if (!textContent.empty()) {
            msgJson["text"] = std::move(textContent);
        }
        msgJson["segments"] = std::move(segments);
        if (!images.empty()) {
            msgJson["images"] = std::move(images);
        }
        if (!faces.empty()) {
            msgJson["faces"] = std::move(faces);
        }
        if (!notifications.empty()) {
            msgJson["notifications"] = std::move(notifications);
        }
        if (m_replyTo > 0) {
            msgJson["reply_to"] = std::to_string(m_replyTo);
        } else {
            msgJson["reply_to"] = nullptr;
        }

        m_recordContent = dumpJson(msgJson);
        co_return;
    }

    const std::string &OneBotMessage::recordContent() const noexcept { return m_recordContent; }

    std::string OneBotMessage::getRawMessage() const { return jsonToString(atOrNull(m_event, "raw_message")); }

    bool OneBotMessage::hasSegment(const std::string_view type) const {
        return std::ranges::any_of(
          atOrNull(m_event, "message"), [type](const json &item) { return getStr(item, "type") == type; });
    }

    void OneBotMessage::setCustomQQName(const uint64_t qqNumber, const std::string &qqName) {
        m_customQQNameMap[qqNumber] = qqName;
        m_QQNameMap[qqNumber] = qqName;
    }

    std::string OneBotMessage::getQQName(const uint64_t qqNumber) {
        if (m_QQNameMap.contains(qqNumber)) {
            return m_QQNameMap[qqNumber];
        }
        return "未知";
    }

    std::unordered_map<std::string, uint64_t> OneBotMessage::getNameToQQMap() {
        std::unordered_map<std::string, uint64_t> nameToQQ;
        for (const auto &[qq, name]: m_QQNameMap) {
            nameToQQ[name] = qq;
        }
        return nameToQQ;
    }
} // namespace insoulforge
