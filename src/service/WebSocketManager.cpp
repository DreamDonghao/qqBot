/// @file WebSocketManager.cpp
/// @brief WebSocket 连接管理器 - 实现

#include <service/WebSocketManager.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <ctime>

namespace LittleMeowBot {
    WebSocketManager& WebSocketManager::instance(){
        static WebSocketManager mgr;
        return mgr;
    }

    void WebSocketManager::addConnection(const drogon::WebSocketConnectionPtr& conn){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.insert(conn);
        spdlog::info("WebSocket连接已建立，当前连接数: {}", m_connections.size());
    }

    void WebSocketManager::removeConnection(const drogon::WebSocketConnectionPtr& conn){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.erase(conn);
        // 清理订阅关系
        for (auto& [groupId, subscribers] : m_subscriptions) {
            subscribers.erase(conn);
        }
        spdlog::info("WebSocket连接已断开，当前连接数: {}", m_connections.size());
    }

    void WebSocketManager::subscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscriptions[groupId].insert(conn);
        spdlog::info("WebSocket订阅群: {}", groupId);
    }

    void WebSocketManager::unsubscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId){
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_subscriptions.contains(groupId)) {
            m_subscriptions[groupId].erase(conn);
        }
    }

    void WebSocketManager::pushMessage(uint64_t groupId, const std::string& role, const std::string& content){
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

    void WebSocketManager::pushGroupList(const std::vector<uint64_t>& groups){
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

    size_t WebSocketManager::getConnectionCount() const{
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connections.size();
    }

    std::string WebSocketManager::getCurrentTimestamp() const{
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        return buffer;
    }
}