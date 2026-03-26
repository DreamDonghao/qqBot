#include "qqBot_QQMessages.h"
#include <ApiClient.hpp>
#include <BotMemory.hpp>
#include <CommandHandler.hpp>
#include <Config.hpp>
#include <GroupConfigManager.hpp>
#include <MemoryService.hpp>
#include <MessageService.hpp>
#include <Prompt.hpp>
#include <QQMessage.hpp>
#include <fmt/core.h>
#include <optional>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <tool.h>

using namespace qqBot;
using namespace drogon;

// 全局状态
Prompt g_prompt;
std::unordered_map<Json::UInt64, BotMemory> g_botMemories;
std::unordered_map<uint64_t, int> g_newQQMesCounts;

ProcessQQMessages::ProcessQQMessages(){
    const auto& config = Config::instance();
    auto& groupConfigMgr = GroupConfigManager::instance();

    // 加载群组配置
    groupConfigMgr.loadFromFile("../group_config.json");

    // 初始化 QQ 昵称
    QQMessage::setCustomQQName(config.self_qq_number, "小喵(我)");
    QQMessage::loadQQNameMap("../qq_name_map.json");
    spdlog::info("成功加载QQ昵称文件");

    // 加载记忆文件
    for (const Json::Value qqMemories = loadJson("../bot_memories.json");
         const auto& groupId : qqMemories.getMemberNames()) {
        g_botMemories[std::stoul(groupId)].loadJson(qqMemories[groupId]);
    }
    spdlog::info("成功加载记忆文件");

    // 设置提示词
    g_prompt.setSystemPrompt(R"(自定义)");

    g_prompt.setRemindPrompt(R"(自定义)");
}

ProcessQQMessages::~ProcessQQMessages(){
    auto& groupConfigMgr = GroupConfigManager::instance();

    // 保存群组配置
    groupConfigMgr.saveToFile("../group_config.json");

    // 保存昵称文件
    QQMessage::saveQQNameMap("../qq_name_map.json");
    spdlog::info("成功保存昵称文件");

    // 保存记忆文件
    Json::Value qqMemories;
    for (auto& [groupId, qqMemory] : g_botMemories) {
        qqMemories[std::to_string(groupId)] = qqMemory.getJson();
    }
    writeJsonAtomic("../bot_memories.json", qqMemories);
    spdlog::info("成功保存记忆文件");
}

Task<> ProcessQQMessages::receiveMessages(
    const HttpRequestPtr req,
    std::function<void(const HttpResponsePtr&)> callback) const{
    const auto json = req->getJsonObject();
    if (!json || !json->isObject()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid JSON or not an object");
        callback(resp);
        co_return;
    }

    // 检查 post_type 字段
    if (!json->isMember("post_type") || (*json)["post_type"].asString() != "message") {
        Json::Value resp;
        resp["status"] = "ok";
        callback(HttpResponse::newHttpJsonResponse(resp));
        co_return;
    }

    // 返回响应
    Json::Value respJson;
    respJson["status"] = "ok";
    callback(HttpResponse::newHttpJsonResponse(respJson));

    auto& config = Config::instance();
    auto& groupConfigMgr = GroupConfigManager::instance();
    auto& apiClient = ApiClient::instance();
    auto& memoryService = MemoryService::instance();
    auto& messageService = MessageService::instance();

    QQMessage qqMessage(*json);
    uint64_t groupId = qqMessage.getGroupId();

    // 只处理指定群
    if (groupId != 1055898774 && groupId != 764139472 && groupId != 1137909452 && groupId != 174898099) {
        co_return;
    }

    // 确保群配置存在
    if (!groupConfigMgr.contains(groupId)) {
        groupConfigMgr.addConfig(groupId);
    }

    // 格式化消息并更新记忆
    co_await qqMessage.formatMessage();
    if (qqMessage.getSelfQQNumber() == qqMessage.getSenderQQNumber()) {
        g_botMemories[groupId].addAssistantChatRecord(qqMessage.getFormatMessage());
    }
    else {
        g_botMemories[groupId].addUserChatRecord(qqMessage.getFormatMessage());
    }

    // 处理消息回复
    if (qqMessage.atMe()) {
        spdlog::info("at类型消息，强制回复");
        auto repMes = co_await apiClient.requestDeepSeek(
            g_prompt.getPrompt(g_botMemories[groupId]),
            config.model_temperature, config.model_top_p,
            config.model_max_tokens);

        if (repMes) {
            co_await messageService.sendGroupMsg(groupId, repMes.value(), g_botMemories);
        }
    }
    else {
        spdlog::info("普通类型消息，AI判断是否回复");

        // 构建判断提示词
        Json::Value judgeMessages;
        Json::Value systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = R"prompt(判断是否需要回复，只回复YES或NO。

输入格式：[日期]表示时间 | {名称}:发言 | @{名称}:at | [图片/表情]特殊消息
"小喵(我)"是机器人自己

YES - 需要回复：

NO - 不需要回复：

只回复YES或NO)prompt";
        judgeMessages.append(systemMsg);

        Json::Value userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = g_botMemories[groupId].getChatRecordsText();
        judgeMessages.append(userMsg);

        auto judgeResult = co_await apiClient.requestDeepSeek(
            judgeMessages,
            0.3f, // 低温度使判断更稳定
            0.9f,
            10); // 只需要简短回复

        bool shouldReply = judgeResult.has_value() &&
        (judgeResult.value().find("YES") != std::string::npos ||
            judgeResult.value() == "YES");

        if (shouldReply) {
            spdlog::info("AI判断需要回复: {}", judgeResult.value_or("无返回"));
            auto repMes = co_await apiClient.requestDeepSeek(
                g_prompt.getPrompt(g_botMemories[groupId]),
                config.model_temperature,
                config.model_top_p,
                config.model_max_tokens);

            if (repMes) {
                co_await messageService.sendGroupMsg(groupId, repMes.value(), g_botMemories);
            }
            else {
                LOG_ERROR << " ";
            }
        }
        else {
            spdlog::info("AI判断不需要回复: {}", judgeResult.value_or("无返回"));
        }
    }

    // 更新统计
    groupConfigMgr.incrementMessageCount(groupId, qqMessage.getFormatMessage().size());
    spdlog::info("群聊统计数据 {} :接收总消息数{}条,接收总字符(字节)数{}个",
                 groupId,
                 groupConfigMgr.getConfig(groupId).AllMesCount,
                 groupConfigMgr.getConfig(groupId).AllCharCount);

    // 记忆生成
    if (++g_newQQMesCounts[groupId] > config.memory_trigger_count) {
        spdlog::info("记忆生成开始 {}({})", groupId, g_botMemories[groupId].getChatRecordCount());

        std::string memoryItem = co_await memoryService.collectMemoryItem(g_botMemories[groupId].getChatRecordsText());
        spdlog::info("记忆搜集成功：\n{}", memoryItem);

        std::string integratedMemory = co_await memoryService.integrateMemory(memoryItem,
                                                                              g_botMemories[groupId].
                                                                              getLongTermMemory());

        g_botMemories[groupId].updateLongTermMemory(integratedMemory);
        spdlog::info("记忆整合成功");

        g_newQQMesCounts[groupId] = 0;
    }
}
