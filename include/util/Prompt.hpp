/// @file Prompt.hpp
/// @brief 提示词构建器 - LLM 提示词组装
/// @author donghao
/// @date 2026-01-20
/// @details 构建 LLM 请求的提示词：
///          - 系统提示词：setSystemPrompt()
///          - 提醒提示词：setRemindPrompt()
///          - 完整提示词：getPrompt()（包含记忆、聊天记录）

#ifndef LITTLE_MEOW_BOT_PROMPT_HPP
#define LITTLE_MEOW_BOT_PROMPT_HPP
#include <json/config.h>
#include <json/value.h>

#include <model/ChatRecordManager.hpp>
#include <model/MemoryManager.hpp>

namespace LittleMeowBot {
    class Prompt {
    public:
        Prompt() {
            m_system["role"] = "system";
            m_remind["role"] = "user";
        }

        void setSystemPrompt(const Json::String systemPrompt) {
            m_system["content"] = systemPrompt;
        }

        void setRemindPrompt(const Json::String remindPrompt) {
            m_remind["content"] = remindPrompt;
        }

        [[nodiscard]] Json::Value getPrompt(
            const ChatRecordManager& chatRecords,
            const MemoryManager& memory
        ) const{
            Json::Value prompt;
            // 添加系统提示词
            prompt.append(m_system);

            // 添加全局记忆提示词
            Json::Value longTermMemoryPrompt;
            longTermMemoryPrompt["role"] = "user";
            longTermMemoryPrompt["content"]
                    = "【重要全局记忆 - 你必须牢牢记住并自然融入所有回复中，不要在回复里直接提及或引用这段记忆本身】\n\n"
                      + memory.getMemory()
                      + "\n\n现在请根据以上记忆，继续正常处理下面的对话：";
            prompt.append(longTermMemoryPrompt);

            // 添加聊天记录
            auto chatRecordsJson = chatRecords.getRecords();
            int index = -1;
            for (int i = 0; i < chatRecordsJson.size(); ++i) {
                if (chatRecordsJson[i]["role"] == "assistant") {
                    index = i;
                }
            }
            for (int i = 0; i < chatRecordsJson.size(); ++i) {
                if (i == index) {
                    prompt.append(chatRecordsJson[i]);
                    prompt.append(m_remind);
                }else {
                    prompt.append(chatRecordsJson[i]);
                }
            }
            return prompt;
        }

    private:
        Json::Value m_system;
        Json::Value m_remind;
    };
} // LittleMeowBot

#endif //LITTLE_MEOW_BOT_PROMPT_HPP
