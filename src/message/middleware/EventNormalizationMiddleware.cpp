/// @file EventNormalizationMiddleware.cpp
/// @brief OneBot 事件归一化中间件实现

#include <atomic>
#include <config/Config.hpp>
#include <message/MessageContext.hpp>
#include <message/middleware/EventNormalizationMiddleware.hpp>
#include <model/OneBotMessage.hpp>
#include <service/OneBotClient.hpp>
#include <util/JsonUtil.hpp>

namespace insoulforge {
    namespace {
        std::atomic nextSyntheticMessageId{9'100'000'000LL};

        /// @brief 为通知事件分配不会与 OneBot 实际消息冲突的合成消息 ID
        [[nodiscard]] int64_t syntheticMessageId() { return nextSyntheticMessageId.fetch_add(1); }

        /// @brief 获取通知参与者的显示名称
        /// @details 优先使用已缓存昵称；缓存未命中时回退 OneBot 查询，失败时保留“未知”。
        drogon::Task<std::string> resolveDisplayName(const uint64_t qq, const uint64_t sessionId) {
            if (std::string name = OneBotMessage::getQQName(qq); name != "未知") {
                co_return name;
            }
            const json response = co_await OneBotClient::getStrangerInfo(qq, sessionId);
            const std::string name = getStr(atOrNull(response, "data"), "nickname");
            co_return name.empty() ? "未知" : name;
        }

        /// @brief 构造所有合成通知消息的公共字段
        [[nodiscard]] json createNotificationMessage(const json &notice, const uint64_t senderId) {
            json event;
            event["post_type"] = "message";
            event["self_id"] = Config::instance().selfQQNumber;
            event["time"] = getInt(notice, "time", static_cast<int>(std::time(nullptr)));
            event["message_id"] = syntheticMessageId();
            event["raw_message"] = "";
            event["sender"]["user_id"] = senderId;
            event["sender"]["nickname"] = OneBotMessage::getQQName(senderId);

            if (const uint64_t groupId = getUInt(notice, "group_id", 0); groupId != 0) {
                event["message_type"] = "group";
                event["group_id"] = groupId;
            } else {
                event["message_type"] = "private";
                event["user_id"] = senderId;
            }
            return event;
        }

        /// @brief 将拍一拍通知转换为独立的 `poke` 消息段
        drogon::Task<json> normalizePoke(const json &notice) {
            const uint64_t actorId = getUInt(notice, "user_id", 0);
            const uint64_t targetId = getUInt(notice, "target_id", 0);
            if (actorId == 0 || targetId == 0 || actorId == Config::instance().selfQQNumber) {
                co_return json();
            }

            const uint64_t sessionId = getUInt(notice, "group_id", 0) != 0
                                         ? getUInt(notice, "group_id", 0)
                                         : actorId | OneBotMessage::kPrivateSessionFlag;
            json event = createNotificationMessage(notice, actorId);
            event["message"].push_back({{"type", "poke"},
              {"data", {{"actor_id", actorId}, {"actor_name", co_await resolveDisplayName(actorId, sessionId)},
                         {"target_id", targetId}, {"target_name", co_await resolveDisplayName(targetId, sessionId)}}}});
            co_return event;
        }

        /// @brief 将群成员变动通知转换为独立的 `notification` 消息段
        [[nodiscard]] json normalizeMembershipChange(const json &notice) {
            const uint64_t memberId = getUInt(notice, "user_id", 0);
            if (const uint64_t groupId = getUInt(notice, "group_id", 0); memberId == 0 || groupId == 0) {
                return {};
            }

            const std::string kind = getStr(notice, "notice_type") == "group_increase" ? "member_join" : "member_leave";
            json event = createNotificationMessage(notice, memberId);
            event["message"].push_back({{"type", "notification"},
              {"data",
                {{"kind", kind}, {"member_id", memberId}, {"member_name", OneBotMessage::getQQName(memberId)},
                  {"operator_id", getUInt(notice, "operator_id", 0)}, {"sub_type", getStr(notice, "sub_type")}}}});
            return event;
        }
    } // namespace

    std::string_view EventNormalizationMiddleware::id() const noexcept { return "event_normalization"; }

    drogon::Task<MessageFlow> EventNormalizationMiddleware::handle(MessageContext &context) const {
        json &event = context.event();
        if (getStr(event, "post_type") == "message") {
            co_return MessageFlow::Continue;
        }

        if (getStr(event, "notice_type") == "notify" && getStr(event, "sub_type") == "poke") {
            event = co_await normalizePoke(event);
        } else if (const std::string noticeType = getStr(event, "notice_type");
          noticeType == "group_increase" || noticeType == "group_decrease") {
            event = normalizeMembershipChange(event);
        } else {
            event = json();
        }
        co_return event.is_null() ? MessageFlow::Stop : MessageFlow::Continue;
    }
} // namespace insoulforge
