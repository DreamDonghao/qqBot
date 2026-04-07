/// @file GroupConfigManager.hpp
/// @brief 群组配置管理器
#pragma once
#include <storage/Database.hpp>

namespace LittleMeowBot {
    class GroupConfigManager{
    public:
        static GroupConfigManager& instance();
        [[nodiscard]] GroupConfig getConfig(uint64_t groupId) const;
        [[nodiscard]] bool contains(uint64_t groupId) const;
        void addConfig(uint64_t groupId, const GroupConfig& config = GroupConfig()) const;
        void incrementMessageCount(uint64_t groupId, size_t charCount) const;

    private:
        GroupConfigManager() = default;
    };
}