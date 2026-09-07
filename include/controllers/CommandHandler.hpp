/// @file CommandHandler.hpp
/// @brief 命令处理器 - QQ 群命令解析与执行
#pragma once
#include <drogon/utils/coroutine.h>
#include <model/OneBotMessage.hpp>
#include <string>

namespace insoulforge {
    [[nodiscard]] bool isCommand(const OneBotMessage &message);

    drogon::Task<std::string> handleCommand(OneBotMessage message);
} // namespace insoulforge
