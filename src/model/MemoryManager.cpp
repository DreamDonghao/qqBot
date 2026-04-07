/// @file MemoryManager.cpp
/// @brief 短期记忆管理器 - 实现

#include <model/MemoryManager.hpp>

namespace LittleMeowBot {
    MemoryManager::MemoryManager(uint64_t groupId) : m_groupId(groupId){}

    void MemoryManager::setGroupId(uint64_t groupId){
        m_groupId = groupId;
    }

    uint64_t MemoryManager::getGroupId() const{
        return m_groupId;
    }

    void MemoryManager::updateMemory(const std::string& content) const{
        Database::instance().updateLongTermMemory(m_groupId, content);
    }

    std::string MemoryManager::getMemory() const{
        return Database::instance().getLongTermMemory(m_groupId);
    }
}