/// @file GroupConfigManager.hpp
/// @brief 群组配置管理器
/// @author donghao
/// @date 2026-04-02
/// @details 管理群组配置信息，使用 SQLite 持久化存储。
///          支持群组配置的增删改查和消息计数统计。

#pragma once
#include <storage/Database.hpp>


namespace LittleMeowBot {
    /// @brief 群组配置管理类
    /// @details 使用 SQLite 存储群组配置信息
    class GroupConfigManager{
    public:
        static GroupConfigManager& instance(){
            static GroupConfigManager manager;
            return manager;
        }

        /// @brief 获取群组配置
        [[nodiscard]] GroupConfig getConfig(const uint64_t groupId) const{
            return Database::instance().getGroupConfig(groupId);
        }

        /// @brief 检查群组是否存在
        [[nodiscard]] bool contains(uint64_t groupId) const{
            return Database::instance().hasGroupConfig(groupId);
        }

        /// @brief 添加群组配置
        void addConfig(uint64_t groupId, const GroupConfig& config = GroupConfig()) const{
            Database::instance().saveGroupConfig(groupId, config);
        }

        /// @brief 增加消息计数
        void incrementMessageCount(uint64_t groupId, size_t charCount) const{
            Database::instance().incrementMessageCount(groupId, charCount);
        }

    private:
        GroupConfigManager() = default;
    };
}
