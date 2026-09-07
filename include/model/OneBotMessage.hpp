/// @file OneBotMessage.hpp
/// @brief OneBot 入站消息模型
/// @author donghao
/// @date 2026-04-02
/// @details 将 OneBot 消息段转换为可持久化的富内容条目。`text` 只保存文本段；图片、表情和
///          通知事件分别保存到独立字段及有序 `segments`，供历史展示和 Agent 共同使用。

#pragma once
#include <drogon/drogon.h>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <util/JsonUtil.hpp>

namespace insoulforge {
    /// @brief 单条已归一化的 OneBot 入站消息
    class OneBotMessage {
    public:
        /// @brief 从已归一化的 OneBot 消息事件构造模型
        /// @param event OneBot `post_type=message` 事件，通知事件也会在归一化后使用该形态
        explicit OneBotMessage(json event);

        /// @brief 检查是否 @ 了机器人
        /// @return 是否 @ 了机器人
        [[nodiscard]] bool atMe() const;

        /// @brief 判断消息是否含有 QQ 原生表情段
        /// @return 至少存在一个 `face` 段时返回 true
        [[nodiscard]] bool hasFace() const;

        /// @brief 判断消息是否是戳向机器人的拍一拍通知
        /// @return 存在 `poke` 段且目标为机器人时返回 true
        [[nodiscard]] bool isPokeForBot() const;

        /// @brief 判断消息是否为无需 Agent 回复的旁观拍一拍
        /// @return 存在 `poke` 段但目标不是机器人时返回 true
        [[nodiscard]] bool isPassivePoke() const;

        /// @brief 判断消息是否包含群成员变动通知
        /// @return 至少存在一个 `member_join` 或 `member_leave` 通知段时返回 true
        [[nodiscard]] bool hasMembershipNotification() const;

        /// @brief 检查是否高优先级 Agent 消息：@机器人、私聊或系统定时任务触发。
        /// @details 优先级只影响已完成入站处理的 Agent 阶段；不会打断正在执行的图片识别或记录。
        /// @return 是否高优先级
        [[nodiscard]] bool isPriorityMessage() const;

        /// @brief 获取群号
        /// @return 群号
        [[nodiscard]] uint64_t getGroupId() const;

        /// @brief 私聊会话标志位：私聊会话 ID = 用户QQ号 | kPrivateSessionFlag，
        /// 与群号共享同一 uint64 键空间（群号不会用到最高位），下游存储/Map 无需区分
        static constexpr uint64_t kPrivateSessionFlag = 1ULL << 63;

        /// @brief 系统定时任务虚拟账号：现网不存在的保留 QQ 号。
        /// 调度器用它作为 sender 合成【系统定时任务】消息注入消息接口，Router 据此确定性放行
        static constexpr uint64_t kSystemAccountId = 10000000000ULL;

        /// @brief 判断会话 ID 是否为私聊会话
        /// @param sessionId 会话 ID
        /// @return 是否私聊
        [[nodiscard]] static constexpr bool isPrivateSession(const uint64_t sessionId) {
            return (sessionId & kPrivateSessionFlag) != 0;
        }

        /// @brief 将会话 ID 解析为存储用的会话类型与目标 ID（定时任务等按此维度落库）
        /// @param sessionId 会话 ID（私聊带 kPrivateSessionFlag）
        /// @return {sessionType("group"|"private"), targetId(群号或未加标志位的 QQ 号)}
        [[nodiscard]] static std::pair<std::string, uint64_t> parseSessionTarget(const uint64_t sessionId) {
            return {isPrivateSession(sessionId) ? "private" : "group",
              isPrivateSession(sessionId) ? sessionId & ~kPrivateSessionFlag : sessionId};
        }

        /// @brief 是否私聊消息
        /// @return OneBot message_type == "private"
        [[nodiscard]] bool isPrivate() const;

        /// @brief 获取会话 ID（群聊=群号；私聊=用户QQ号|kPrivateSessionFlag）
        /// @return 会话 ID，可作下游存储与并发控制的统一键
        [[nodiscard]] uint64_t getSessionId() const;

        /// @brief 获取机器人自己的 QQ 号
        /// @return 机器人 QQ 号
        [[nodiscard]] uint64_t getSelfQQNumber() const;

        /// @brief 获取发送者 QQ 号
        /// @return 发送者 QQ 号
        [[nodiscard]] uint64_t getSenderQQNumber() const;

        /// @brief 获取消息归属用户的 QQ 号（顶层 user_id 字段，缺失时回退 sender）。
        /// 私聊会话以该字段为准：定时任务合成的私聊事件中 sender 是系统账号，
        /// 会话与回复目标必须指向真实的用户 QQ
        /// @return 用户 QQ 号
        [[nodiscard]] uint64_t getUserId() const;

        /// @brief 获取消息 ID
        /// @return 消息 ID
        [[nodiscard]] uint64_t getMessageId() const;

        /// @brief 丰富消息内容（异步，可能识别图片）
        /// @details 完成后生成可持久化条目。每张图片独立记录识别状态，单张失败不影响其他段。
        drogon::Task<> enrichContent();

        /// @brief 获取可持久化的富内容条目
        /// @pre 已调用 enrichContent()。
        /// @return 紧凑 JSON 字符串
        [[nodiscard]] const std::string &recordContent() const noexcept;

        /// @brief 获取原始消息文本（不含 CQ 码）
        /// @return 原始消息文本
        [[nodiscard]] std::string getRawMessage() const;

        /// @brief 设置自定义 QQ 昵称
        /// @param qqNumber QQ 号
        /// @param qqName 自定义昵称
        static void setCustomQQName(uint64_t qqNumber, const std::string &qqName);

        /// @brief 获取 QQ 昵称
        /// @param qqNumber QQ 号
        /// @return 昵称（优先返回自定义昵称）
        static std::string getQQName(uint64_t qqNumber);

        /// @brief 获取昵称到QQ号的反向映射（用于@转换）
        /// @return 昵称到QQ号的映射表
        static std::unordered_map<std::string, uint64_t> getNameToQQMap();

    private:
        /// @brief 获取发送者昵称
        /// @return 发送者昵称
        [[nodiscard]] std::string getSenderQQName() const;

        /// @brief 判断原始消息段中是否存在指定类型
        [[nodiscard]] bool hasSegment(std::string_view type) const;

        const json m_event; ///< 已归一化的 OneBot 消息事件
        std::string m_recordContent; ///< enrichContent 生成的富内容 JSON
        uint64_t m_replyTo{0}; ///< 引用的消息ID
        bool m_isAtMe{false}; ///< 是否 @ 了机器人

        inline static std::unordered_map<uint64_t, std::string> m_QQNameMap; ///< QQ 号到昵称映射
        inline static std::unordered_map<uint64_t, std::string> m_customQQNameMap; ///< 自定义昵称映射
    };
} // namespace insoulforge
