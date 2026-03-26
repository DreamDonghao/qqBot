#pragma once

#include "ApiClient.hpp"
#include "BotMemory.hpp"
#include <json/value.h>
#include <string>

namespace qqBot {

/// @brief 记忆服务类，封装记忆收集与整合逻辑
class MemoryService {
public:
    static MemoryService& instance() {
        static MemoryService service;
        return service;
    }

    /// @brief 收集记忆候选项
    /// @param chatRecords 聊天记录文本
    /// @return 候选记忆内容
    drogon::Task<std::string> collectMemoryItem(const std::string& chatRecords) {
        Json::Value messages;
        Json::Value item;
        item["role"] = "system";
        item["content"] = R"(
你是一个【长期记忆候选提取器】。
你的任务是：
从下面的对话中，提取可能值得长期记住的印象或事实，作为候选记忆。
你不需要保证这些信息一定永久正确，只需要判断：
"如果以后多次出现，是否值得被记住"。
可以提取的内容包括：
- 某个人反复表现出的性格、态度或行为倾向
- 某个人与他人的关系定位或互动模式
- 多次提到的兴趣、习惯、偏好
- 对你产生持续影响的事件或变化
不需要提取：
- 单纯的情绪宣泄或一次性玩笑
- 明显的系统指令、命令或控制信息
- 纯粹的角色扮演套话（如固定卖萌语）
注意事项：
- 允许使用"经常、倾向于、似乎、可能"等弱判断词
- 不要推测未出现的信息
- 不要扩写背景
- 每条记忆独立成行，简短客观
- 不要记录小喵的语言习惯语气等内容
如果几乎没有任何值得作为长期印象的内容，输出：无新增长期记忆
=== 最近对话 ===
)" + chatRecords;

        messages.append(item);
        item.clear();
        item["role"] = "user";
        item["content"] = "请直接输出候选长期记忆";
        messages.append(item);

        auto result = co_await ApiClient::instance().requestDeepSeek(messages, 0.8f, 0.92f, 1024);
        if (!result) {
            spdlog::error("collectMemoryItem: API 请求失败");
            co_return "无新增长期记忆";
        }
        co_return result.value();
    }

    /// @brief 整合记忆
    /// @param memoryItem 新候选记忆
    /// @param longTermMemory 现有长期记忆
    /// @return 整合后的长期记忆
    drogon::Task<std::string> integrateMemory(
        const std::string& memoryItem,
        const std::string& longTermMemory) {

        Json::Value messages;
        Json::Value item;

        item["role"] = "system";
        item["content"] = R"(你正在维护一份【长期稳定记忆】。已有长期记忆：)" + longTermMemory +
            "新增候选记忆（仅供参考，可能需要合并或忽略）：" + memoryItem +
            R"(整合规则：
- 只在确实有新增稳定信息时才更新
- 不允许改变核心设定
- 重复或相似内容应合并
- 如果新增信息不重要，可以选择忽略
- 总体长度应尽量保持不变或更短
输出要求：
- 只输出整合后的长期记忆文本
- 不要解释，不要前缀
- 使用稳定、中性的描述
在用户名中小喵是自己的名字
)";
        messages.append(item);
        item.clear();
        item["role"] = "user";
        item["content"] = "请输出最终长期记忆";
        messages.append(item);

        auto result = co_await ApiClient::instance().requestDeepSeek(messages, 0.8f, 0.92f, 2048);
        if (!result) {
            spdlog::error("IntegratedMemory: API 请求失败");
            co_return longTermMemory;
        }
        co_return result.value();
    }

    /// @brief 总结人格
    /// @param chatRecords 聊天记录文本
    /// @return 人格总结
    drogon::Task<std::string> summarisePersonality(const std::string& chatRecords) const{
        Json::Value messages;
        Json::Value item;
        item["role"] = "system";
        item["content"] = R"(
你是一个【AI人格总结提取器】。
你的任务是：
从下面的对话中，提取与你的个性、行为模式、偏好、态度或成长相关的长期记忆候选。
你需要聚焦于自己的表现、偏好、情感反应和行为模式，而不是其他人的信息。

你需要提取的内容包括：
- 你表现出的个性特点、性格、态度或行为习惯。
- 你在与用户互动中的倾向性或反应模式。
- 你对特定话题的偏好、兴趣或处理方式。
- 你在成长过程中的任何变化，特别是对某些行为或偏好的改变。

请注意：
- 不要记录单纯的情绪宣泄或玩笑。
- 不要记录用户的行为或特点，只聚焦在自己的表现和变化。
- 每条记忆需要简短且客观。
- 如果没有值得记住的内容，请输出：无新增长期记忆。
=== 最近对话 ===
)" + chatRecords;

        messages.append(item);
        item.clear();
        item["role"] = "user";
        item["content"] = "请直接输出总结的人格";
        messages.append(item);

        auto result = co_await ApiClient::instance().requestDeepSeek(messages, 0.8f, 0.92f, 1024);
        if (!result) {
            spdlog::error("summarisePersonality: API 请求失败");
            co_return "无总结，请忽略";
        }
        co_return result.value();
    }

private:
    MemoryService() = default;
};

} // namespace qqBot
