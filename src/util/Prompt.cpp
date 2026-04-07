/// @file Prompt.cpp
/// @brief 提示词构建器 - 实现

#include <util/Prompt.hpp>

namespace LittleMeowBot {
    Prompt::Prompt(){
        m_system["role"] = "system";
        m_remind["role"] = "user";
    }

    void Prompt::setSystemPrompt(const Json::String systemPrompt){
        m_system["content"] = systemPrompt;
    }

    void Prompt::setRemindPrompt(const Json::String remindPrompt){
        m_remind["content"] = remindPrompt;
    }

    Json::Value Prompt::getPrompt(
        const ChatRecordManager& chatRecords,
        const MemoryManager& memory) const{
        Json::Value prompt;
        prompt.append(m_system);

        Json::Value longTermMemoryPrompt;
        longTermMemoryPrompt["role"] = "user";
        longTermMemoryPrompt["content"]
            = "【重要全局记忆 - 你必须牢牢记住并自然融入所有回复中，不要在回复里直接提及或引用这段记忆本身】\n\n"
            + memory.getMemory()
            + "\n\n现在请根据以上记忆，继续正常处理下面的对话：";
        prompt.append(longTermMemoryPrompt);

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
            } else {
                prompt.append(chatRecordsJson[i]);
            }
        }
        return prompt;
    }
}