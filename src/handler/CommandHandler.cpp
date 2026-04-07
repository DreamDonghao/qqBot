/// @file CommandHandler.cpp
/// @brief 命令处理器 - 实现

#include <handler/CommandHandler.hpp>
#include <model/QQMessage.hpp>
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <sstream>
#include <vector>
#include <model/GroupConfigManager.hpp>
namespace LittleMeowBot {
    CommandHandler& CommandHandler::instance(){
        static CommandHandler handler;
        return handler;
    }

    bool CommandHandler::isCommand(const QQMessage& message) const{
        if (!message.atMe()) return false;
        std::string rawMsg = message.getRawMessage();

        size_t pos = 0;
        while (pos < rawMsg.length()) {
            while (pos < rawMsg.length() && std::isspace(rawMsg[pos])) {
                pos++;
            }
            if (pos < rawMsg.length() && rawMsg[pos] == '[') {
                if (const size_t end = rawMsg.find(']', pos);
                    end != std::string::npos) {
                    pos = end + 1;
                    continue;
                }
            }
            break;
        }

        return pos < rawMsg.length() && rawMsg[pos] == '/';
    }

    drogon::Task<std::string> CommandHandler::handleCommand(
        const QQMessage& message,
        ChatRecordManager& chatRecords) const{
        std::string rawMsg = message.getRawMessage();
        uint64_t groupId = message.getGroupId();
        uint64_t senderQQ = message.getSenderQQNumber();

        auto& database = Database::instance();
        bool hasPermission = database.isAdmin(senderQQ);

        std::string cmdStr;
        size_t pos = 0;
        while (pos < rawMsg.length()) {
            while (pos < rawMsg.length() && std::isspace(rawMsg[pos])) {
                pos++;
            }
            if (pos < rawMsg.length() && rawMsg[pos] == '[') {
                if (size_t end = rawMsg.find(']', pos);
                    end != std::string::npos) {
                    pos = end + 1;
                    continue;
                }
            }
            if (pos < rawMsg.length() && rawMsg[pos] == '/') {
                cmdStr = rawMsg.substr(pos);
                break;
            }
            break;
        }

        std::istringstream iss(cmdStr);
        std::string cmd;
        iss >> cmd;

        std::string response;

        if (cmd == "/help" || cmd == "/帮助") {
            response = "可用命令:\n"
                "【群聊管理】\n"
                "/enable [群号] - 启用群聊\n"
                "/disable [群号] - 禁用群聊\n"
                "/groups - 查看启用的群列表\n"
                "/status - 查看当前群状态\n"
                "【管理员】\n"
                "/admins - 查看管理员列表\n"
                "/addadmin <QQ号> - 添加管理员\n"
                "/deladmin <QQ号> - 移除管理员\n"
                "【表情管理】\n"
                "/addemoji <名称> <路径> - 添加表情\n"
                "/delemoji <名称> - 删除表情\n"
                "/listemoji - 查看表情列表\n"
                "【其他】\n"
                "/help - 显示帮助\n"
                "/about - 关于本项目\n\n"
                "注意: 管理命令仅限管理员使用";
        } else if (cmd == "/status" || cmd == "/状态") {
            bool enabled = database.isGroupEnabled(groupId);
            auto [allMesCount, allCharCount] = GroupConfigManager::instance().getConfig(groupId);
            response = fmt::format(
                "群 {} 状态:\n"
                "- 启用: {}\n"
                "- 消息数: {}\n"
                "- 字符数: {}",
                groupId,
                enabled ? "是" : "否",
                allMesCount,
                allCharCount
            );
        } else if (cmd == "/admins" || cmd == "/管理员") {
            auto admins = database.getAdmins();
            response = "管理员列表:\n";
            for (auto qq : admins) {
                response += fmt::format("- {}\n", qq);
            }
            if (admins.empty()) {
                response = "暂无管理员";
            }
        } else if (cmd == "/about" || cmd == "/关于") {
            response = "LittleMeowBot - 智能 QQ 群聊机器人\n"
                "基于 Agent 架构，支持自定义角色、长期记忆、多工具调用\n\n"
                "项目地址: https://github.com/DreamDonghao/LittleMeowBot\n"
                "作者: DreamDonghao\n"
                "许可证: AGPL-3.0 (未经允许禁止商用)";
        } else if (!hasPermission) {
            response = fmt::format("权限不足，你({})不是管理员", senderQQ);
        } else if (cmd == "/enable" || cmd == "/启用") {
            uint64_t targetGroup = groupId;
            if (std::string arg; iss >> arg) {
                try {
                    targetGroup = std::stoull(arg);
                } catch (...) {
                    co_return "无效的群号格式";
                }
            }
            database.enableGroup(targetGroup);
            response = fmt::format("已启用群: {}", targetGroup);
        } else if (cmd == "/disable" || cmd == "/禁用") {
            uint64_t targetGroup = groupId;
            if (std::string arg; iss >> arg) {
                try {
                    targetGroup = std::stoull(arg);
                } catch (...) {
                    co_return "无效的群号格式";
                }
            }
            database.disableGroup(targetGroup);
            response = fmt::format("已禁用群: {}", targetGroup);
        } else if (cmd == "/groups" || cmd == "/群列表") {
            auto groups = database.getEnabledGroups();
            response = "启用的群聊列表:\n";
            for (auto gid : groups) {
                response += fmt::format("- {}\n", gid);
            }
            if (groups.empty()) {
                response = "没有启用的群聊";
            }
        } else if (cmd == "/addadmin" || cmd == "/添加管理员") {
            std::string arg;
            if (!(iss >> arg)) {
                co_return "用法: /addadmin <QQ号>";
            }
            try {
                uint64_t qq = std::stoull(arg);
                database.addAdmin(qq);
                response = fmt::format("已添加管理员: {}", qq);
            } catch (...) {
                response = "无效的QQ号格式";
            }
        } else if (cmd == "/deladmin" || cmd == "/移除管理员") {
            std::string arg;
            if (!(iss >> arg)) {
                co_return "用法: /deladmin <QQ号>";
            }
            try {
                uint64_t qq = std::stoull(arg);
                database.removeAdmin(qq);
                response = fmt::format("已移除管理员: {}", qq);
            } catch (...) {
                response = "无效的QQ号格式";
            }
        } else if (cmd == "/addemoji" || cmd == "/添加表情") {
            std::string name, path;
            if (!(iss >> name >> path)) {
                co_return "用法: /addemoji <名称> <路径>\n例如: /addemoji happy /home/emojis/happy.gif";
            }
            database.addEmoji(name, path);
            response = fmt::format("已添加表情: {} -> {}", name, path);
        } else if (cmd == "/delemoji" || cmd == "/删除表情") {
            std::string name;
            if (!(iss >> name)) {
                co_return "用法: /delemoji <名称>";
            }
            database.removeEmoji(name);
            response = fmt::format("已删除表情: {}", name);
        } else if (cmd == "/listemoji" || cmd == "/表情列表") {
            auto emojis = database.getAllEmojis();
            if (emojis.empty()) {
                response = "表情库为空\n添加表情: /addemoji <名称> <路径>";
            } else {
                response = "表情库列表:\n";
                for (const auto& [name, path] : emojis) {
                    response += fmt::format("- {} : {}\n", name, path);
                }
                response += fmt::format("\n共 {} 个表情", emojis.size());
            }
        } else {
            response = fmt::format("未知命令: {}\n使用 /help 查看可用命令", cmd);
        }

        co_return response;
    }
}