//
// Created by donghao on 2026/1/20.
//
#ifndef QQ_BOT_BOTMEMORY_HPP
#define QQ_BOT_BOTMEMORY_HPP
#include <deque>
#include <json/config.h>
#include <json/value.h>
#include <mutex>
#include <shared_mutex>


namespace qqBot {
    class BotMemory {
    public:
        BotMemory() = default;

        explicit BotMemory(Json::Value botMemory) {
            std::unique_lock lock_c(m_chatRecordsMutex);
            std::unique_lock lock_l(m_longTermMemoryMutex);
            for (auto &item: botMemory["chatRecord"]) {
                m_chatRecords.emplace_back(std::move(item));
            }
            m_LongTermMemory = std::move(botMemory["longTermMemory"].asString());
        }

        void loadJson(Json::Value botMemory) {
            for (auto &item: botMemory["chatRecord"]) {
                m_chatRecords.emplace_back(std::move(item));
            }
            m_LongTermMemory = std::move(botMemory["longTermMemory"].asString());
        }

        Json::Value getJson() const {
            std::shared_lock lock_c(m_chatRecordsMutex);
            std::shared_lock lock_l(m_longTermMemoryMutex);
            Json::Value retJson;
            retJson["longTermMemory"] = getLongTermMemory();
            retJson["chatRecord"] = getChatRecordsJson();
            return retJson;
        }

        ~BotMemory() = default;

        void addUserChatRecord(const Json::String &chatRecord) {
            std::unique_lock lock(m_chatRecordsMutex);
            Json::Value chatRecordJson;
            chatRecordJson["role"] = "user";
            chatRecordJson["content"] = chatRecord;
            m_chatRecords.emplace_back(std::move(chatRecordJson));
        }

        void addAssistantChatRecord(const Json::String& chatRecord) {
            std::unique_lock lock(m_chatRecordsMutex);
            Json::Value chatRecordJson;
            chatRecordJson["role"] = "assistant";
            chatRecordJson["content"] = chatRecord;
            m_chatRecords.emplace_back(std::move(chatRecordJson));
        }

        Json::Value getChatRecordsJson() const {
            std::shared_lock lock(m_chatRecordsMutex);
            Json::Value chatRecordsJson;
            for (const auto &chatRecordJson: m_chatRecords) {
                chatRecordsJson.append(chatRecordJson);
            }
            return chatRecordsJson;
        }

        /// @brief 获取聊天记录格式化字符串
        /// @details 格式:\n
        /// {user0}:message0\n
        /// {user1}:message1\n
        /// {回复}:message\n
        /// {user2}:message2
        Json::String getChatRecordsText() const {
            std::shared_lock lock(m_chatRecordsMutex);
            Json::String chatRecordsText;
            for (const auto &chatRecordJson: m_chatRecords) {
                // chatRecordsText.append(chatRecordJson.asString() + "\n");
                if (chatRecordJson["role"] == "user") {
                    chatRecordsText.append(chatRecordJson["content"].asString() + "\n");
                }else if (chatRecordJson["role"] == "assistant") {
                    chatRecordsText.append("{小喵(我)}:"+chatRecordJson["content"].asString() + "\n");
                }
            }
            return chatRecordsText;
        }

        std::deque<Json::Value> getChatRecords() const {
            std::shared_lock lock(m_chatRecordsMutex);
            return m_chatRecords;
        }

        void updateLongTermMemory(Json::String longTermMemory) {
            std::unique_lock lock(m_longTermMemoryMutex);
            m_LongTermMemory = std::move(longTermMemory);
            while (m_chatRecords.size() > 12) {
                m_chatRecords.pop_front();
            }
        }

        Json::String getLongTermMemory() const {
            std::shared_lock lock(m_longTermMemoryMutex);
            return m_LongTermMemory;
        }

        size_t getChatRecordCount() const {
            std::shared_lock lock(m_chatRecordsMutex);
            return m_chatRecords.size();
        }

    private:
        mutable std::shared_mutex m_chatRecordsMutex;
        std::deque<Json::Value> m_chatRecords; ///< 聊天记录
        mutable std::shared_mutex m_longTermMemoryMutex;
        Json::String m_LongTermMemory; ///< 长时记忆
    };
} // namespace qqBot

#endif // QQ_BOT_BOTMEMORY_HPP
