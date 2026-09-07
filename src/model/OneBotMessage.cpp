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
            co_return description.empty() ? std::nullopt : std::optional{description};
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

        json images = json::array();
        json segments = json::array();
        for (const auto &item: atOrNull(m_event, "message")) {
            const std::string type = getStr(item, "type");
            const json &data = atOrNull(item, "data");
            if (type == "text") {
                const std::string text = getStr(data, "text");
                segments.push_back({{"type", "text"}, {"text", text}});
            } else if (type == "at") {
                const uint64_t atQQ = parseUInt64(jsonToString(atOrNull(atOrNull(item, "data"), "qq")));
                if (atQQ == 0) {
                    segments.push_back({{"type", "at"}, {"target", {{"kind", "all"}}}});
                } else {
                    json target{{"qq", std::to_string(atQQ)}};
                    if (const std::string name = getQQName(atQQ); name != "未知") {
                        target["name"] = name;
                    }
                    segments.push_back({{"type", "at"}, {"target", std::move(target)}});
                }
            } else if (type == "face") {
                json face;
                face["type"] = "face";
                face["id"] = getStr(data, "id");
                if (const std::string label = getStr(atOrNull(data, "raw"), "faceText"); !label.empty()) {
                    face["label"] = label;
                }
                segments.push_back(std::move(face));
            } else if (type == "image") {
                const size_t imageIndex = images.size();
                json image;
                image["source"] = {{"file", getStr(data, "file")}, {"url", getStr(data, "url")}};
                if (const auto description = co_await describeImage(getStr(data, "url"), getSessionId())) {
                    image["recognition_status"] = "succeeded";
                    image["description"] = *description;
                } else {
                    image["recognition_status"] = "failed";
                }
                images.push_back(image);
                segments.push_back({{"type", "image"}, {"image_index", imageIndex}});
            } else if (type == "poke") {
                json target{{"qq", jsonToString(atOrNull(data, "target_id"))}};
                if (const std::string name = getStr(data, "target_name"); !name.empty() && name != "未知") {
                    target["name"] = name;
                }
                segments.push_back({{"type", "poke"}, {"target", std::move(target)}, {"direction", "inbound"}});
            } else if (type == "notification") {
                std::string action = getStr(data, "kind");
                if (action.starts_with("member_")) {
                    action.erase(0, std::string_view("member_").size());
                }
                json event{{"type", "member_event"}, {"action", std::move(action)}};
                if (const std::string reason = getStr(data, "sub_type"); !reason.empty()) {
                    event["reason"] = reason;
                }
                if (const std::string operatorId = jsonToString(atOrNull(data, "operator_id")); !operatorId.empty()) {
                    event["operator"] = {{"qq", operatorId}};
                }
                segments.push_back(std::move(event));
            } else if (type != "reply") {
                segments.push_back({{"type", "unsupported"}, {"segment_type", type}});
            }
        }

        json msgJson;
        msgJson["time"] = timeStr;
        msgJson["sender"]["name"] = senderName;
        msgJson["sender"]["qq"] = std::to_string(senderQQ);
        msgJson["message_id"] = std::to_string(msgId);
        msgJson["segments"] = std::move(segments);
        if (!images.empty()) {
            msgJson["assets"]["images"] = std::move(images);
        }
        if (m_replyTo > 0) {
            msgJson["reply_to"] = std::to_string(m_replyTo);
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
