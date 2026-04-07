/// @file Prompt.hpp
/// @brief 提示词构建器
#pragma once
#include <json/config.h>
#include <json/value.h>
#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>

namespace LittleMeowBot {
    class Prompt{
    public:
        Prompt();

        void setSystemPrompt(const Json::String systemPrompt);
        void setRemindPrompt(const Json::String remindPrompt);

        [[nodiscard]] Json::Value getPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory) const;

    private:
        Json::Value m_system;
        Json::Value m_remind;
    };
}