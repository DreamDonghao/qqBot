//
// Created by donghao on 2026/3/18.
//
#ifndef QQ_BOT_GROUPCONFIGMANAGER_HPP
#define QQ_BOT_GROUPCONFIGMANAGER_HPP

#include <QQMessage.hpp>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <string>

namespace qqBot {

/// @brief 群组配置结构
struct GroupConfig {
    double probability = 0.25;    ///< 回复率
    uint64_t newMesCounts = 0;    ///< 新消息统计
    uint64_t AllMesCount = 0;     ///< 收到消息总数
    uint64_t AllCharCount = 0;    ///< 收到字符总数
};

/// @brief 群组配置管理类
class GroupConfigManager {
public:
    static GroupConfigManager& instance() {
        static GroupConfigManager manager;
        return manager;
    }

    /// @brief 加载配置文件
    void loadFromFile(const std::string& filepath) {
        Json::Value groupConfigs = loadJson(filepath);
        m_configs.clear();
        for (const auto& key : groupConfigs.getMemberNames()) {
            uint64_t gid = std::stoull(key);
            const Json::Value& v = groupConfigs[key];
            GroupConfig cfg;
            cfg.probability = v.get("probability", 0.25).asDouble();
            cfg.newMesCounts = v.get("newMesCounts", 0).asUInt64();
            cfg.AllMesCount = v.get("AllMesCount", 0).asUInt64();
            cfg.AllCharCount = v.get("AllCharCount", 0).asUInt64();
            m_configs.emplace(gid, cfg);
        }
        spdlog::info("成功加载群聊配置文件");
    }

    /// @brief 保存配置文件
    void saveToFile(const std::string& filepath) {
        Json::Value out(Json::objectValue);
        for (const auto& [gid, cfg] : m_configs) {
            Json::Value v;
            v["probability"] = cfg.probability;
            v["newMesCounts"] = cfg.newMesCounts;
            v["AllMesCount"] = cfg.AllMesCount;
            v["AllCharCount"] = cfg.AllCharCount;
            out[std::to_string(gid)] = v;
        }
        writeJsonAtomic(filepath, out);
    }

    /// @brief 获取群组配置
    /// @param groupId 群号
    /// @return 群组配置引用
    GroupConfig& getConfig(uint64_t groupId) {
        if (!m_configs.contains(groupId)) {
            m_configs[groupId] = GroupConfig();
        }
        return m_configs[groupId];
    }

    /// @brief 检查群组是否存在
    bool contains(uint64_t groupId) const {
        return m_configs.contains(groupId);
    }

    /// @brief 添加群组配置
    void addConfig(uint64_t groupId, const GroupConfig& config = GroupConfig()) {
        m_configs[groupId] = config;
    }

    /// @brief 增加消息计数
    void incrementMessageCount(uint64_t groupId, size_t charCount) {
        ++m_configs[groupId].AllMesCount;
        m_configs[groupId].AllCharCount += charCount;
    }

private:
    GroupConfigManager() = default;
    std::unordered_map<uint64_t, GroupConfig> m_configs;
};

} // namespace qqBot

#endif //QQ_BOT_GROUPCONFIGMANAGER_HPP