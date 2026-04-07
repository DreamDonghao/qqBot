/// @file MemoryManager.hpp
/// @brief 短期记忆管理器
#pragma once
#include <storage/Database.hpp>
#include <string>

namespace LittleMeowBot {
    class MemoryManager{
    public:
        explicit MemoryManager(uint64_t groupId);
        void setGroupId(uint64_t groupId);
        uint64_t getGroupId() const;
        void updateMemory(const std::string& content) const;
        [[nodiscard]] std::string getMemory() const;

    private:
        uint64_t m_groupId;
    };
}