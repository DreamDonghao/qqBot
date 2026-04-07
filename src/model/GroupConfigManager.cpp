/// @file GroupConfigManager.cpp
/// @brief 群组配置管理器 - 实现

#include <model/GroupConfigManager.hpp>

namespace LittleMeowBot {
    GroupConfigManager& GroupConfigManager::instance(){
        static GroupConfigManager manager;
        return manager;
    }

    GroupConfig GroupConfigManager::getConfig(uint64_t groupId) const{
        return Database::instance().getGroupConfig(groupId);
    }

    bool GroupConfigManager::contains(uint64_t groupId) const{
        return Database::instance().hasGroupConfig(groupId);
    }

    void GroupConfigManager::addConfig(uint64_t groupId, const GroupConfig& config) const{
        Database::instance().saveGroupConfig(groupId, config);
    }

    void GroupConfigManager::incrementMessageCount(uint64_t groupId, size_t charCount) const{
        Database::instance().incrementMessageCount(groupId, charCount);
    }
}