#include "AdminController.h"
#include <model/QQMessage.hpp>
#include <spdlog/spdlog.h>

using namespace LittleMeowBot;
using namespace drogon;

// ==================== LLM配置 ====================

Task<> AdminController::getLLMConfigs(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto configs = Database::instance().getAllLLMConfigs();
    callback(HttpResponse::newHttpJsonResponse(configs));
    co_return;
}

Task<> AdminController::saveLLMConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("name")) {
        Json::Value err;
        err["error"] = "缺少name字段";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    std::string name = (*json)["name"].asString();
    Database::instance().saveLLMConfig(name, *json);

    // 更新内存中的配置
    auto& config = Config::instance();
    if (name == "router") {
        config.router.apiKey = (*json).get("apiKey", "").asString();
        config.router.baseUrl = (*json).get("baseUrl", "").asString();
        config.router.path = (*json).get("path", "").asString();
        config.router.model = (*json).get("model", "").asString();
        config.routerParams.maxTokens = (*json).get("maxTokens", 100).asInt();
        config.routerParams.temperature = (*json).get("temperature", 0.7f).asFloat();
        config.routerParams.topP = (*json).get("topP", 0.9f).asFloat();
    } else if (name == "planner") {
        config.planner.apiKey = (*json).get("apiKey", "").asString();
        config.planner.baseUrl = (*json).get("baseUrl", "").asString();
        config.planner.path = (*json).get("path", "").asString();
        config.planner.model = (*json).get("model", "").asString();
        config.plannerParams.maxTokens = (*json).get("maxTokens", 100).asInt();
        config.plannerParams.temperature = (*json).get("temperature", 0.7f).asFloat();
        config.plannerParams.topP = (*json).get("topP", 0.9f).asFloat();
    } else if (name == "executor") {
        config.executor.apiKey = (*json).get("apiKey", "").asString();
        config.executor.baseUrl = (*json).get("baseUrl", "").asString();
        config.executor.path = (*json).get("path", "").asString();
        config.executor.model = (*json).get("model", "").asString();
        config.executorParams.maxTokens = (*json).get("maxTokens", 100).asInt();
        config.executorParams.temperature = (*json).get("temperature", 0.7f).asFloat();
        config.executorParams.topP = (*json).get("topP", 0.9f).asFloat();
    } else if (name == "memory") {
        config.memory.apiKey = (*json).get("apiKey", "").asString();
        config.memory.baseUrl = (*json).get("baseUrl", "").asString();
        config.memory.path = (*json).get("path", "").asString();
        config.memory.model = (*json).get("model", "").asString();
        config.memoryParams.maxTokens = (*json).get("maxTokens", 100).asInt();
        config.memoryParams.temperature = (*json).get("temperature", 0.7f).asFloat();
        config.memoryParams.topP = (*json).get("topP", 0.9f).asFloat();
    } else if (name == "image") {
        config.image.apiKey = (*json).get("apiKey", "").asString();
        config.image.baseUrl = (*json).get("baseUrl", "").asString();
        config.image.path = (*json).get("path", "").asString();
        config.image.model = (*json).get("model", "").asString();
    }

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "LLM配置已保存";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 提示词 ====================

Task<> AdminController::getPrompts(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto prompts = Database::instance().getAllPrompts();

    Json::Value result;
    for (const auto& [key, content] : prompts) {
        result[key] = content;
    }
    callback(HttpResponse::newHttpJsonResponse(result));
    co_return;
}

Task<> AdminController::savePrompt(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("key") || !json->isMember("content")) {
        Json::Value err;
        err["error"] = "缺少key或content字段";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    std::string key = (*json)["key"].asString();
    std::string content = (*json)["content"].asString();
    std::string description = json->isMember("description") ? (*json)["description"].asString() : "";

    Database::instance().setPrompt(key, content, description);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "提示词已保存";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 表情库 ====================

Task<> AdminController::getEmojis(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto emojis = Database::instance().getAllEmojis();

    Json::Value result;
    for (const auto& [name, path] : emojis) {
        Json::Value emoji;
        emoji["name"] = name;
        emoji["path"] = path;
        result.append(emoji);
    }
    callback(HttpResponse::newHttpJsonResponse(result));
    co_return;
}

Task<> AdminController::addEmoji(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("name") || !json->isMember("path")) {
        Json::Value err;
        err["error"] = "缺少name或path字段";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    std::string name = (*json)["name"].asString();
    std::string path = (*json)["path"].asString();
    std::string description = json->isMember("description") ? (*json)["description"].asString() : "";

    Database::instance().addEmoji(name, path, description);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "表情已添加";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

Task<> AdminController::removeEmoji(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& name
) const{
    Database::instance().removeEmoji(name);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "表情已删除";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 管理员 ====================

Task<> AdminController::getAdmins(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto admins = Database::instance().getAdmins();

    Json::Value result;
    for (uint64_t qq : admins) {
        Json::Value admin;
        admin["qq"] = static_cast<Json::UInt64>(qq);
        result.append(admin);
    }
    callback(HttpResponse::newHttpJsonResponse(result));
    co_return;
}

Task<> AdminController::addAdmin(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("qq")) {
        Json::Value err;
        err["error"] = "缺少qq字段";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    uint64_t qq = (*json)["qq"].asUInt64();
    Database::instance().addAdmin(qq);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "管理员已添加";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

Task<> AdminController::removeAdmin(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& qq
) const{
    uint64_t qqNum = std::stoull(qq);
    Database::instance().removeAdmin(qqNum);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "管理员已删除";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 启用群 ====================

Task<> AdminController::getGroups(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto groups = Database::instance().getEnabledGroupsWithNames();

    Json::Value result;
    for (const auto& [groupId, groupName] : groups) {
        Json::Value group;
        group["groupId"] = static_cast<Json::UInt64>(groupId);
        group["groupName"] = groupName;
        group["messageCount"] = static_cast<Json::Int>(Database::instance().getChatRecordCount(groupId));
        result.append(group);
    }
    callback(HttpResponse::newHttpJsonResponse(result));
    co_return;
}

Task<> AdminController::enableGroup(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json || !json->isMember("groupId")) {
        Json::Value err;
        err["error"] = "缺少groupId字段";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    uint64_t groupId = (*json)["groupId"].asUInt64();
    Database::instance().enableGroup(groupId);

    // 自动获取群名称
    auto groupName = co_await MessageService::instance().fetchAndUpdateGroupName(groupId);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "群已启用";
    resp["groupName"] = groupName;
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

Task<> AdminController::disableGroup(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& groupId
) const{
    uint64_t gid = std::stoull(groupId);
    Database::instance().disableGroup(gid);

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "群已禁用";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

Task<> AdminController::refreshGroupName(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& groupId
) const{
    uint64_t gid = std::stoull(groupId);
    auto groupName = co_await MessageService::instance().fetchAndUpdateGroupName(gid);

    Json::Value resp;
    resp["success"] = true;
    resp["groupName"] = groupName;
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 聊天记录 ====================

Task<> AdminController::getChatRecords(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& groupId
) const{
    uint64_t gid = std::stoull(groupId);

    // 支持limit参数
    int limit = 50;
    if (req->getParameter("limit") != "") {
        limit = std::stoi(req->getParameter("limit"));
    }

    auto records = Database::instance().getChatRecords(gid, limit);

    Json::Value result;
    for (const auto& record : records) {
        Json::Value item;
        item["role"] = record["role"].asString();
        item["content"] = record["content"].asString();
        result.append(item);
    }
    callback(HttpResponse::newHttpJsonResponse(result));
    co_return;
}

// ==================== 知识库配置 ====================

Task<> AdminController::getKBConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto config = Database::instance().getKBConfig();
    callback(HttpResponse::newHttpJsonResponse(config));
    co_return;
}

Task<> AdminController::saveKBConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json) {
        Json::Value err;
        err["error"] = "缺少配置数据";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    Database::instance().saveKBConfig(*json);

    // 更新内存中的配置
    auto& kbConfig = Config::instance().knowledgeBase;
    kbConfig.apiKey = (*json).get("apiKey", "").asString();
    kbConfig.baseUrl = (*json).get("baseUrl", "").asString();
    kbConfig.knowledgeDatasetId = (*json).get("knowledgeDatasetId", "").asString();
    kbConfig.memoryDatasetId = (*json).get("memoryDatasetId", "").asString();
    kbConfig.memoryDocumentId = (*json).get("memoryDocumentId", "").asString();

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "知识库配置已保存";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 群记忆 ====================

Task<> AdminController::getGroupMemory(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback,
    const std::string& groupId
) const{
    uint64_t gid = std::stoull(groupId);
    std::string memory = Database::instance().getLongTermMemory(gid);

    Json::Value resp;
    resp["groupId"] = static_cast<Json::UInt64>(gid);
    resp["memory"] = memory;
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== 记忆配置 ====================

Task<> AdminController::getMemoryConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto config = Database::instance().getMemoryConfig();
    callback(HttpResponse::newHttpJsonResponse(config));
    co_return;
}

Task<> AdminController::saveMemoryConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json) {
        Json::Value err;
        err["error"] = "缺少配置数据";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    Database::instance().saveMemoryConfig(*json);

    // 更新内存中的配置
    auto& config = Config::instance();
    config.memoryTriggerCount = (*json)["memoryTriggerCount"].asInt();
    config.memoryChatRecordLimit = (*json)["memoryChatRecordLimit"].asInt();
    config.shortTermMemoryMax = (*json)["shortTermMemoryMax"].asInt();
    config.shortTermMemoryLimit = (*json)["shortTermMemoryLimit"].asInt();
    config.memoryMigrateCount = (*json)["memoryMigrateCount"].asInt();

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "记忆配置已保存";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}

// ==================== QQ Bot 配置 ====================

Task<> AdminController::getQQConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto config = Database::instance().getQQConfig();
    callback(HttpResponse::newHttpJsonResponse(config));
    co_return;
}

Task<> AdminController::saveQQConfig(
    HttpRequestPtr req,
    std::function<void(const HttpResponsePtr &)> callback
) const{
    auto json = req->getJsonObject();
    if (!json) {
        Json::Value err;
        err["error"] = "缺少配置数据";
        callback(HttpResponse::newHttpJsonResponse(err));
        co_return;
    }

    Database::instance().saveQQConfig(*json);

    // 更新内存中的配置
    auto& config = Config::instance();
    config.accessToken = (*json).get("accessToken", "").asString();
    config.selfQQNumber = (*json).get("selfQQNumber", 0).asInt64();
    config.qqHttpHost = (*json).get("qqHttpHost", "").asString();
    config.botName = (*json).get("botName", "小喵").asString();

    // 更新 QQMessage 的自定义名称
    QQMessage::setCustomQQName(config.selfQQNumber, config.botName + "(我)");

    Json::Value resp;
    resp["success"] = true;
    resp["message"] = "QQ Bot 配置已保存";
    callback(HttpResponse::newHttpJsonResponse(resp));
    co_return;
}