/// @file WebSocketManager.hpp
/// @brief WebSocket 连接管理器
#pragma once
#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <vector>
#include <string>

namespace LittleMeowBot {
    class WebSocketManager{
    public:
        static WebSocketManager& instance();

        void addConnection(const drogon::WebSocketConnectionPtr& conn);
        void removeConnection(const drogon::WebSocketConnectionPtr& conn);
        void subscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId);
        void unsubscribeGroup(const drogon::WebSocketConnectionPtr& conn, uint64_t groupId);
        void pushMessage(uint64_t groupId, const std::string& role, const std::string& content);
        void pushGroupList(const std::vector<uint64_t>& groups);
        size_t getConnectionCount() const;

    private:
        WebSocketManager() = default;

        std::string getCurrentTimestamp() const;

        std::unordered_set<drogon::WebSocketConnectionPtr> m_connections;
        std::unordered_map<uint64_t, std::unordered_set<drogon::WebSocketConnectionPtr>> m_subscriptions;
        mutable std::mutex m_mutex;
    };
}