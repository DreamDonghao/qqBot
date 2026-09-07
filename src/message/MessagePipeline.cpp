/// @file MessagePipeline.cpp
/// @brief 入站消息处理链路实现

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include <message/MessageContext.hpp>
#include <message/MessageMiddleware.hpp>
#include <message/MessageMiddlewareCatalog.hpp>
#include <message/MessagePipeline.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <model/OneBotMessage.hpp>
#include <util/Logger.hpp>

namespace insoulforge {
    namespace {
        /// @brief 从 OneBot 消息或通知事件提取会话 ID
        /// @return 无法归属会话的事件返回 0
        [[nodiscard]] uint64_t eventSessionId(const json &event) {
            if (const uint64_t groupId = getUInt(event, "group_id", 0); groupId != 0) {
                return groupId;
            }
            const uint64_t userId = getUInt(event, "user_id", getUInt(atOrNull(event, "sender"), "user_id", 0));
            return userId == 0 ? 0 : userId | OneBotMessage::kPrivateSessionFlag;
        }
    } // namespace

    MessagePipeline &MessagePipeline::instance() {
        static MessagePipeline pipeline;
        return pipeline;
    }

    void MessagePipeline::initialize() {
        initialize(createBuiltinMessageRuntime(), MessageMiddlewareCatalog::createBuiltinMiddlewares());
    }

    void MessagePipeline::initialize(std::vector<std::unique_ptr<MessageMiddleware>> middlewares) {
        initialize(createBuiltinMessageRuntime(), std::move(middlewares));
    }

    void MessagePipeline::initialize(
      std::shared_ptr<const MessageRuntime> runtime, std::vector<std::unique_ptr<MessageMiddleware>> middlewares) {
        if (m_initialized) {
            return;
        }
        if (!runtime) {
            throw std::invalid_argument("消息运行时不能为空");
        }

        // 先在局部容器中完成全部校验，校验失败时不污染正在使用的链路。
        std::unordered_set<std::string_view> ids;
        for (const auto &middleware: middlewares) {
            if (!middleware || middleware->id().empty() || !ids.insert(middleware->id()).second) {
                throw std::invalid_argument("内置消息中间件无效或标识重复");
            }
        }
        m_middlewares = std::move(middlewares);
        m_runtime = std::move(runtime);
        m_initialized = true;
    }

    void MessagePipeline::addMiddleware(std::unique_ptr<MessageMiddleware> middleware) {
        ensureInitialized();
        validateMiddleware(middleware);
        m_middlewares.push_back(std::move(middleware));
    }

    void MessagePipeline::insertBefore(const std::string_view anchorId, std::unique_ptr<MessageMiddleware> middleware) {
        ensureInitialized();
        validateMiddleware(middleware);
        const size_t index = findMiddlewareIndex(anchorId);
        m_middlewares.insert(m_middlewares.begin() + static_cast<std::ptrdiff_t>(index), std::move(middleware));
    }

    void MessagePipeline::insertAfter(const std::string_view anchorId, std::unique_ptr<MessageMiddleware> middleware) {
        ensureInitialized();
        validateMiddleware(middleware);
        const size_t index = findMiddlewareIndex(anchorId);
        m_middlewares.insert(m_middlewares.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(middleware));
    }

    void MessagePipeline::enqueue(json event) {
        ensureInitialized();
        const uint64_t sessionId = eventSessionId(event);
        if (sessionId == 0) {
            drogon::async_run([this, event = std::move(event)]() mutable -> drogon::Task<> {
                co_await process(std::move(event));
            });
            return;
        }

        bool shouldStartDraining = false;
        {
            std::lock_guard lock(m_queueMutex);
            auto &[events, draining] = m_sessionQueues[sessionId];
            events.push_back(std::move(event));
            if (!draining) {
                draining = true;
                shouldStartDraining = true;
            }
        }
        if (shouldStartDraining) {
            drogon::async_run([this, sessionId]() -> drogon::Task<> { co_await drainSession(sessionId); });
        }
    }

    void MessagePipeline::ensureInitialized() const {
        if (!m_initialized) {
            throw std::logic_error("必须先初始化内置消息中间件");
        }
    }

    void MessagePipeline::validateMiddleware(const std::unique_ptr<MessageMiddleware> &middleware) const {
        if (!middleware || middleware->id().empty()) {
            throw std::invalid_argument("消息中间件及其标识不能为空");
        }
        if (std::ranges::any_of(
              m_middlewares, [&middleware](const auto &existing) { return existing->id() == middleware->id(); })) {
            throw std::invalid_argument("消息中间件标识重复");
        }
    }

    size_t MessagePipeline::findMiddlewareIndex(const std::string_view anchorId) const {
        if (anchorId.empty()) {
            throw std::invalid_argument("中间件锚点标识不能为空");
        }
        const auto it = std::ranges::find_if(
          m_middlewares, [anchorId](const auto &middleware) { return middleware->id() == anchorId; });
        if (it == m_middlewares.end()) {
            throw std::out_of_range("未找到中间件锚点");
        }
        return static_cast<size_t>(it - m_middlewares.begin());
    }

    drogon::Task<> MessagePipeline::process(json event) const {
        ensureInitialized();
        MessageContext context(std::move(event), m_runtime);
        for (const auto &middleware: m_middlewares) {
            try {
                // Stop 是正常的短路结果，例如非消息事件、命令或禁用会话。
                if (co_await middleware->handle(context) == MessageFlow::Stop) {
                    co_return;
                }
            } catch (const std::exception &error) {
                Logger::session(context.logSessionId())
                  .error("[MessagePipeline] 节点 {} 处理失败: message_id={}, error={}", middleware->id(),
                    context.logMessageId(), error.what());
                co_return;
            } catch (...) {
                Logger::session(context.logSessionId())
                  .error("[MessagePipeline] 节点 {} 处理失败: message_id={}, 未知异常", middleware->id(),
                    context.logMessageId());
                co_return;
            }
        }
    }

    drogon::Task<> MessagePipeline::drainSession(const uint64_t sessionId) {
        while (true) {
            json event;
            {
                std::lock_guard lock(m_queueMutex);
                const auto queueIt = m_sessionQueues.find(sessionId);
                if (queueIt == m_sessionQueues.end() || queueIt->second.events.empty()) {
                    m_sessionQueues.erase(sessionId);
                    co_return;
                }
                event = std::move(queueIt->second.events.front());
                queueIt->second.events.pop_front();
            }
            co_await process(std::move(event));
        }
    }
} // namespace insoulforge
