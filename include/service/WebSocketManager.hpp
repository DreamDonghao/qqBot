/// @file WebSocketManager.hpp
/// @brief WebSocket 连接管理器
/// @author donghao
/// @date 2026-04-02
/// @details 管理 Web 管理后台的 WebSocket 连接：
///          - 连接管理：添加、移除连接
///          - 群订阅：支持按群订阅消息推送
///          - 消息推送：实时推送新消息和群列表更新

#pragma once
#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

namespace LittleMeowBot {
    /// @brief WebSocket连接管理器
/// @details 管理所有WebSocket连接，支持按群订阅消息
    class WebSocketManager{
    public:
        static WebSocketManager& instance(){
            static WebSocketManager mgr;
            return mgr;
        }

        /// @brief 添加连接
        void addConnection(const drogon::WebSocketConnectionPtr& conn){
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connections.insert(conn);
            spdlog::info("WebSocket连接已建立，当前连接数: {}", m_connections.size());
        }

        /// @brief 移除连接
        void removeConnection(const drogon::WebSocketConnectionPtr& conn){
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connections.erase(conn);
            // 清理订阅关系
            for (auto& [groupId, subscribers] : m_subscriptions) {
                subscribers.erase(conn);
            }
            spdlog::info("WebSocket连接已断开，当前连接数: {}", m_connections.size());
        }

        /// @brief 订阅特定群的消息
        void subscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId){
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subscriptions[groupId].insert(conn);
            spdlog::info("WebSocket订阅群: {}", groupId);
        }

        /// @brief 取消订阅群
        void unsubscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId){
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_subscriptions.contains(groupId)) {
                m_subscriptions[groupId].erase(conn);
            }
        }

        /// @brief 推送新消息到订阅该群的连接
        void pushMessage(uint64_t groupId, const std::string& role, const std::string& content){
            std::lock_guard<std::mutex> lock(m_mutex);

            Json::Value msg;
            msg["type"] = "new_message";
            msg["groupId"] = static_cast<Json::UInt64>(groupId);
            msg["data"]["role"] = role;
            msg["data"]["content"] = content;
            msg["data"]["timestamp"] = getCurrentTimestamp();

            Json::StreamWriterBuilder builder;
            std::string jsonStr = Json::writeString(builder, msg);

            // 发送给订阅该群的连接
            if (m_subscriptions.contains(groupId)) {
                for (const auto& conn : m_subscriptions[groupId]) {
                    conn->send(jsonStr);
                }
            }

            // 也发送给未订阅特定群的连接（订阅所有群）
            for (const auto& conn : m_connections) {
                // 如果连接没有订阅任何群，发送所有消息
                bool hasSubscription = false;
                for (const auto& [gid, subscribers] : m_subscriptions) {
                    if (subscribers.contains(conn)) {
                        hasSubscription = true;
                        break;
                    }
                }
                if (!hasSubscription) {
                    conn->send(jsonStr);
                }
            }
        }

        /// @brief 推送群列表更新
        void pushGroupList(const std::vector<uint64_t>& groups){
            std::lock_guard<std::mutex> lock(m_mutex);

            Json::Value msg;
            msg["type"] = "group_list";
            for (uint64_t groupId : groups) {
                msg["groups"].append(static_cast<Json::UInt64>(groupId));
            }

            Json::StreamWriterBuilder builder;
            std::string jsonStr = Json::writeString(builder, msg);

            for (const auto& conn : m_connections) {
                conn->send(jsonStr);
            }
        }

        /// @brief 获取当前连接数
        size_t getConnectionCount() const{
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_connections.size();
        }

    private:
        WebSocketManager() = default;

        std::string getCurrentTimestamp() const{
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&time);
            char buffer[20];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
            return buffer;
        }

        std::unordered_set<drogon::WebSocketConnectionPtr> m_connections;
        std::unordered_map<uint64_t, std::unordered_set<drogon::WebSocketConnectionPtr>> m_subscriptions;
        mutable std::mutex m_mutex;
    };
}