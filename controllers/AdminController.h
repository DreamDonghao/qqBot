/// @file AdminController.h
/// @brief 管理后台 REST API 控制器
/// @author donghao
/// @date 2026-04-02

#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>
#include <storage/Database.hpp>
#include <service/MessageService.hpp>

namespace LittleMeowBot {
    /// @brief 管理后台 REST API 控制器
    /// @details 提供 Web 管理界面的所有 API 接口，包括：
    ///          - LLM 配置管理
    ///          - 提示词管理
    ///          - 表情库管理
    ///          - 管理员管理
    ///          - 启用群管理
    ///          - 知识库配置
    ///          - 聊天记录查看
    ///          - 记忆系统配置
    class AdminController : public drogon::HttpController<AdminController> {
    public:
        METHOD_LIST_BEGIN
        // LLM 配置
        ADD_METHOD_TO(AdminController::getLLMConfigs, "/admin/api/llm-configs", drogon::Get);
        ADD_METHOD_TO(AdminController::saveLLMConfig, "/admin/api/llm-config", drogon::Post);
        // 提示词
        ADD_METHOD_TO(AdminController::getPrompts, "/admin/api/prompts", drogon::Get);
        ADD_METHOD_TO(AdminController::savePrompt, "/admin/api/prompt", drogon::Post);
        // 表情库
        ADD_METHOD_TO(AdminController::getEmojis, "/admin/api/emojis", drogon::Get);
        ADD_METHOD_TO(AdminController::addEmoji, "/admin/api/emoji", drogon::Post);
        ADD_METHOD_TO(AdminController::removeEmoji, "/admin/api/emoji/{name}", drogon::Delete);
        // 管理员
        ADD_METHOD_TO(AdminController::getAdmins, "/admin/api/admins", drogon::Get);
        ADD_METHOD_TO(AdminController::addAdmin, "/admin/api/admin", drogon::Post);
        ADD_METHOD_TO(AdminController::removeAdmin, "/admin/api/admin/{qq}", drogon::Delete);
        // 启用群
        ADD_METHOD_TO(AdminController::getGroups, "/admin/api/groups", drogon::Get);
        ADD_METHOD_TO(AdminController::enableGroup, "/admin/api/group", drogon::Post);
        ADD_METHOD_TO(AdminController::disableGroup, "/admin/api/group/{groupId}", drogon::Delete);
        ADD_METHOD_TO(AdminController::refreshGroupName, "/admin/api/group/{groupId}/refresh-name", drogon::Post);
        // 知识库配置
        ADD_METHOD_TO(AdminController::getKBConfig, "/admin/api/kb-config", drogon::Get);
        ADD_METHOD_TO(AdminController::saveKBConfig, "/admin/api/kb-config", drogon::Post);
        // 聊天记录
        ADD_METHOD_TO(AdminController::getChatRecords, "/admin/api/chat-records/{groupId}", drogon::Get);
        // 群记忆
        ADD_METHOD_TO(AdminController::getGroupMemory, "/admin/api/memory/{groupId}", drogon::Get);
        // 记忆配置
        ADD_METHOD_TO(AdminController::getMemoryConfig, "/admin/api/memory-config", drogon::Get);
        ADD_METHOD_TO(AdminController::saveMemoryConfig, "/admin/api/memory-config", drogon::Post);
        // QQ Bot 配置
        ADD_METHOD_TO(AdminController::getQQConfig, "/admin/api/qq-config", drogon::Get);
        ADD_METHOD_TO(AdminController::saveQQConfig, "/admin/api/qq-config", drogon::Post);
        METHOD_LIST_END

        // ============== LLM 配置 ==============

        /// @brief 获取所有 LLM 配置
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getLLMConfigs(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 保存指定 LLM 配置
        /// @param req HTTP 请求，body 包含配置 JSON
        /// @param callback HTTP 响应回调
        drogon::Task<> saveLLMConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        // ============== 提示词 ==============

        /// @brief 获取所有提示词
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getPrompts(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 保存提示词
        /// @param req HTTP 请求，body 包含 key 和 content
        /// @param callback HTTP 响应回调
        drogon::Task<> savePrompt(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        // ============== 表情库 ==============

        /// @brief 获取所有表情
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getEmojis(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 添加表情
        /// @param req HTTP 请求，body 包含 name 和 path
        /// @param callback HTTP 响应回调
        drogon::Task<> addEmoji(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 删除表情
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        /// @param name 表情名称
        drogon::Task<> removeEmoji(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& name) const;

        // ============== 管理员 ==============

        /// @brief 获取管理员列表
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getAdmins(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 添加管理员
        /// @param req HTTP 请求，body 包含 qq
        /// @param callback HTTP 响应回调
        drogon::Task<> addAdmin(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 删除管理员
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        /// @param qq 管理员 QQ 号
        drogon::Task<> removeAdmin(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& qq) const;

        // ============== 启用群 ==============

        /// @brief 获取启用群列表
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getGroups(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 启用群
        /// @param req HTTP 请求，body 包含 groupId
        /// @param callback HTTP 响应回调
        drogon::Task<> enableGroup(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 禁用群
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        /// @param groupId 群号
        drogon::Task<> disableGroup(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& groupId) const;

        /// @brief 刷新群名称
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        /// @param groupId 群号
        drogon::Task<> refreshGroupName(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& groupId) const;

        // ============== 知识库配置 ==============

        /// @brief 获取知识库配置
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getKBConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 保存知识库配置
        /// @param req HTTP 请求，body 包含配置 JSON
        /// @param callback HTTP 响应回调
        drogon::Task<> saveKBConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        // ============== 聊天记录 ==============

        /// @brief 获取群聊天记录
        /// @param req HTTP 请求，可选 limit 参数
        /// @param callback HTTP 响应回调
        /// @param groupId 群号
        drogon::Task<> getChatRecords(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& groupId) const;

        // ============== 群记忆 ==============

        /// @brief 获取群短期记忆
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        /// @param groupId 群号
        drogon::Task<> getGroupMemory(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            const std::string& groupId) const;

        // ============== 记忆配置 ==============

        /// @brief 获取记忆系统配置
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getMemoryConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 保存记忆系统配置
        /// @param req HTTP 请求，body 包含配置 JSON
        /// @param callback HTTP 响应回调
        drogon::Task<> saveMemoryConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        // ============== QQ Bot 配置 ==============

        /// @brief 获取 QQ Bot 配置
        /// @param req HTTP 请求
        /// @param callback HTTP 响应回调
        drogon::Task<> getQQConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

        /// @brief 保存 QQ Bot 配置
        /// @param req HTTP 请求，body 包含配置 JSON
        /// @param callback HTTP 响应回调
        drogon::Task<> saveQQConfig(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;
    };
} // namespace LittleMeowBot