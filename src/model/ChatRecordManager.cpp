/// @file ChatRecordManager.cpp
/// @brief 聊天记录管理器 - 实现

#include <model/ChatRecordManager.hpp>
#include <config/Config.hpp>
namespace LittleMeowBot {
    ChatRecordManager::ChatRecordManager(uint64_t groupId) : m_groupId(groupId){}

    void ChatRecordManager::setGroupId(uint64_t groupId){
        m_groupId = groupId;
    }

    uint64_t ChatRecordManager::getGroupId() const{
        return m_groupId;
    }

    void ChatRecordManager::addUserRecord(const std::string& content) const{
        Database::instance().addChatRecord(m_groupId, "user", content);
    }

    void ChatRecordManager::addAssistantRecord(const std::string& content) const{
        Database::instance().addChatRecord(m_groupId, "assistant", content);
    }

    Json::Value ChatRecordManager::getRecordsJson() const{
        Json::Value recordsJson;
        for (const auto records = Database::instance().getChatRecords(m_groupId);
             const auto& record : records) {
            recordsJson.append(record);
        }
        return recordsJson;
    }

    std::string ChatRecordManager::getRecordsText() const{
        return Database::instance().getChatRecordsText(
            m_groupId,
            Config::instance().memoryChatRecordLimit,
            Config::instance().botName);
    }

    std::deque<Json::Value> ChatRecordManager::getRecords() const{
        std::deque<Json::Value> result;
        for (const auto records = Database::instance().getChatRecords(m_groupId);
             const auto& record : records) {
            result.push_back(record);
        }
        return result;
    }

    size_t ChatRecordManager::getRecordCount() const{
        return Database::instance().getChatRecordCount(m_groupId);
    }

    void ChatRecordManager::clearOldRecords() const{
        Database::instance().clearOldRecords(m_groupId, Config::instance().memoryChatRecordLimit);
    }
}