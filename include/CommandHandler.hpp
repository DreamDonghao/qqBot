//
// Created by donghao on 2026/3/18.
//
#ifndef QQ_BOT_COMMANDHANDLER_HPP
#define QQ_BOT_COMMANDHANDLER_HPP

#include "Config.hpp"
#include "MessageService.hpp"
#include "GroupConfigManager.hpp"
#include <drogon/utils/coroutine.h>
#include <fmt/core.h>
#include <string>
#include <vector>

namespace qqBot {

/// @brief 命令处理类，封装命令解析与执行逻辑
class CommandHandler {
public:
    static CommandHandler& instance() {
        static CommandHandler handler;
        return handler;
    }

private:
    CommandHandler() = default;

    std::vector<std::string> split(const std::string& str, const std::string& delim) const{
        std::vector<std::string> result;
        if (delim.empty()) {
            result.push_back(str);
            return result;
        }
        std::size_t pos = 0;
        while (true) {
            const auto next = str.find(delim, pos);
            if (next == std::string::npos) {
                result.emplace_back(str.substr(pos));
                break;
            }
            result.emplace_back(str.substr(pos, next - pos));
            pos = next + delim.size();
        }
        return result;
    }
};

} // namespace qqBot

#endif //QQ_BOT_COMMANDHANDLER_HPP