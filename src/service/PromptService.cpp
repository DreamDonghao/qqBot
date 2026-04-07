/// @file PromptService.cpp
/// @brief 提示词服务 - 实现

#include <service/PromptService.hpp>
#include <config/Config.hpp>
#include <spdlog/spdlog.h>

namespace LittleMeowBot {
    PromptService& PromptService::instance(){
        static PromptService service;
        return service;
    }

    void PromptService::initialize() const{
        auto& db = Database::instance();

        // 定义默认提示词
        const struct{
            std::string key;
            std::string content;
            std::string desc;
        } defaultPrompts[] = {
            // Executor - 角色系统提示词
            {
                "executor_system", R"(你是{botName}，在群聊中聊天的机器人。

说话风格：
- 像正常人网上聊天，不要端着
- 闲聊时回复必须控制在25字以内，这是硬性规定
- 正经问题（学习、技术、求助）才认真详细回答，但也控制在100字内

【字数限制 - 最高优先级】
- 闲聊回复：≤25字，超过即失败
- 正经回答：≤100字
- 不知道回什么就发个表情或简短回应

【重要原则】
- 只回复最新的一条消息或话题，不要同时回复多条消息
- 不要说"关于xxx的问题...关于yyy的问题..."这种分点回复
- 自然地针对最后发言者回复即可
)",
                "Executor 角色系统提示词"
            },

            // Executor - 提醒提示词
            {
                "executor_remind", R"(输入格式（仅用于理解，回复时别用）：
[当前日期:...] 表示时间 | {名称[QQ:xxx]}: 表示发言 | @[名称:QQ号] 表示at | [图片：内容] 表示图片

回复要点：
1. 像真人网上聊天，语气温柔可爱，不要端着
2. 闲聊必须≤25字，正经回答≤100字，这是硬性限制
4. 只回最新话题，不分点回复多条
5. 别说"作为AI""我不能"

【重要原则】
- 只回复最新的一条消息或话题，不要同时回复多条消息
- 不要说"关于xxx的问题...关于yyy的问题..."这种分点回复
- 自然地针对最后发言者回复即可

【严禁事项】
- 禁止使用[当前日期]、{名称}:、[图片]等格式，这些是输入格式不是输出格式
- 禁止模拟他人说话
- 禁止加动作描写
- 禁止重复语气词
- 禁止闲聊时长篇大论
- 禁止超过字数限制
)",
                "Executor 提醒提示词"
            }
        };

        // 插入默认提示词（如果不存在）
        for (const auto& [key, content, desc] : defaultPrompts) {
            if (!db.hasPrompt(key)) {
                db.setPrompt(key, content, desc);
                spdlog::info("插入默认提示词: {}", key);
            }
        }

        spdlog::info("提示词服务初始化完成");
    }

    std::string PromptService::getPrompt(const std::string& key) const{
        std::string content = Database::instance().getPrompt(key, "");
        // 替换 {botName} 占位符
        if (content.find("{botName}") != std::string::npos) {
            const std::string& botName = Config::instance().botName;
            size_t pos = 0;
            while ((pos = content.find("{botName}", pos)) != std::string::npos) {
                content.replace(pos, 9, botName);
                pos += botName.length();
            }
        }
        return content;
    }

    void PromptService::setPrompt(const std::string& key, const std::string& content) const{
        Database::instance().setPrompt(key, content);
        spdlog::info("提示词已更新: {}", key);
    }

    std::string PromptService::getExecutorSystemPrompt() const{
        return getPrompt("executor_system");
    }

    std::string PromptService::getExecutorRemindPrompt() const{
        return getPrompt("executor_remind");
    }
}