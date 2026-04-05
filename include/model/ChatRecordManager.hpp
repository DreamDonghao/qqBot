/// @file ChatRecordManager.hpp
/// @brief 聊天记录管理器
/// @author donghao
/// @date 2026-04-02
/// @details 管理单个群组的聊天记录，使用 SQLite 持久化存储。
///          支持用户消息和 AI 回复的记录与检索。

#pragma once

#include <storage/Database.hpp>
#include <config/Config.hpp>
#include <json/value.h>
#include <deque>
#include <vector>

namespace LittleMeowBot {
    /// @brief 聊天记录管理类
    /// @details 管理单个群组的聊天记录，使用 SQLite 存储。
    ///          每个群组对应一个 ChatRecordManager 实例。
    class ChatRecordManager {
    public:
        /// @brief 构造函数
        /// @param groupId 群号
        explicit ChatRecordManager(uint64_t groupId) : m_groupId(groupId) {}

        /// @brief 设置群号
        /// @param groupId 群号
        void setGroupId(uint64_t groupId) {
            m_groupId = groupId;
        }

        /// @brief 获取群号
        /// @return 群号
        uint64_t getGroupId() const {
            return m_groupId;
        }

        /// @brief 添加用户消息记录
        /// @param content 消息内容
        void addUserRecord(const std::string& content) const {
            Database::instance().addChatRecord(m_groupId, "user", content);
        }

        /// @brief 添加 AI 回复记录
        /// @param content 回复内容
        void addAssistantRecord(const std::string& content) const {
            Database::instance().addChatRecord(m_groupId, "assistant", content);
        }

        /// @brief 获取聊天记录 JSON 数组
        /// @return JSON 数组，每条记录包含 role 和 content 字段
        [[nodiscard]] Json::Value getRecordsJson() const {
            Json::Value recordsJson;
            for (const auto records = Database::instance().getChatRecords(m_groupId);
                 const auto& record : records) {
                recordsJson.append(record);
            }
            return recordsJson;
        }

        /// @brief 获取聊天记录文本
        /// @return 格式化的聊天记录文本
        [[nodiscard]] std::string getRecordsText() const {
            return Database::instance().getChatRecordsText(
                m_groupId,
                Config::instance().memoryChatRecordLimit,
                Config::instance().botName
            );
        }

        /// @brief 获取聊天记录双端队列
        /// @return 聊天记录队列
        [[nodiscard]] std::deque<Json::Value> getRecords() const {
            std::deque<Json::Value> result;
            for (const auto records = Database::instance().getChatRecords(m_groupId);
                 const auto& record : records) {
                result.push_back(record);
            }
            return result;
        }

        /// @brief 获取聊天记录数量
        /// @return 记录数量
        [[nodiscard]] size_t getRecordCount() const {
            return Database::instance().getChatRecordCount(m_groupId);
        }

        /// @brief 清理旧记录
        /// @details 保留最近 N 条记录，删除其余记录
        void clearOldRecords() const {
            Database::instance().clearOldRecords(m_groupId, Config::instance().memoryChatRecordLimit);
        }

    private:
        uint64_t m_groupId;  ///< 群号
    };
} // namespace LittleMeowBot