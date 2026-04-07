/// @file ChatRecordManager.hpp
/// @brief 聊天记录管理器
#pragma once
#include <storage/Database.hpp>
#include <json/value.h>
#include <deque>


namespace LittleMeowBot {
    class ChatRecordManager{
    public:
        explicit ChatRecordManager(uint64_t groupId);
        void setGroupId(uint64_t groupId);
        uint64_t getGroupId() const;
        void addUserRecord(const std::string& content) const;
        void addAssistantRecord(const std::string& content) const;
        [[nodiscard]] Json::Value getRecordsJson() const;
        [[nodiscard]] std::string getRecordsText() const;
        [[nodiscard]] std::deque<Json::Value> getRecords() const;
        [[nodiscard]] size_t getRecordCount() const;
        void clearOldRecords() const;

    private:
        uint64_t m_groupId;
    };
}
