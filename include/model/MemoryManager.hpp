/// @file MemoryManager.hpp
/// @brief 短期记忆管理器
/// @author donghao
/// @date 2026-04-02
/// @details 管理单个群组的短期记忆，支持存储、更新和检索操作。
///          短期记忆以纯文本形式存储，每行一条记忆条目。

#pragma once

#include <storage/Database.hpp>
#include <string>

namespace LittleMeowBot {
    /// @brief 短期记忆管理类
    /// @details 管理单个群组的短期记忆，每行一条记忆条目。
    ///          记忆格式示例：
    ///          @code
    ///          用户A喜欢用Python写脚本
    ///          用户B最近在学Rust
    ///          群里讨论过项目架构
    ///          @endcode
    class MemoryManager {
    public:
        /// @brief 构造函数
        /// @param groupId 群号
        explicit MemoryManager(uint64_t groupId) : m_groupId(groupId) {}

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

        /// @brief 更新短期记忆
        /// @param content 记忆内容（每行一条）
        void updateMemory(const std::string& content) const {
            Database::instance().updateLongTermMemory(m_groupId, content);
        }

        /// @brief 获取短期记忆
        /// @return 记忆内容（每行一条）
        [[nodiscard]] std::string getMemory() const {
            return Database::instance().getLongTermMemory(m_groupId);
        }

    private:
        uint64_t m_groupId;  ///< 群号
    };
} // namespace LittleMeowBot