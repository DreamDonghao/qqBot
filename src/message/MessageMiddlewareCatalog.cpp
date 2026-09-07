/// @file MessageMiddlewareCatalog.cpp
/// @brief 内置入站消息中间件注册

#include <message/MessageMiddlewareCatalog.hpp>
#include <message/middleware/AgentAvailabilityMiddleware.hpp>
#include <message/middleware/AgentReplyMiddleware.hpp>
#include <message/middleware/CommandDetectionMiddleware.hpp>
#include <message/middleware/CommandExecutionMiddleware.hpp>
#include <message/middleware/EventNormalizationMiddleware.hpp>
#include <message/middleware/FormatMessageMiddleware.hpp>
#include <message/middleware/MessageSetupMiddleware.hpp>
#include <message/middleware/PostProcessMiddleware.hpp>
#include <message/middleware/RecordMessageMiddleware.hpp>
#include <message/middleware/SessionEnabledMiddleware.hpp>

namespace insoulforge {
    std::vector<std::unique_ptr<MessageMiddleware>> MessageMiddlewareCatalog::createBuiltinMiddlewares() {
        std::vector<std::unique_ptr<MessageMiddleware>> middlewares;
        // 顺序是行为契约：命令识别先于会话开关，记录先于命令执行和 Agent。

        // 过滤无关事件，并将拍一拍、群成员变动转换为富通知消息
        middlewares.emplace_back(std::make_unique<EventNormalizationMiddleware>());
        // Agent 未运行时终止消息处理链路
        middlewares.emplace_back(std::make_unique<AgentAvailabilityMiddleware>());
        // 创建 OneBotMessage，并确保对应会话配置存在
        middlewares.emplace_back(std::make_unique<MessageSetupMiddleware>());
        // 识别命令消息，执行阶段位于记录之后
        middlewares.emplace_back(std::make_unique<CommandDetectionMiddleware>());
        // 忽略未启用会话中的非命令消息
        middlewares.emplace_back(std::make_unique<SessionEnabledMiddleware>());
        // 执行消息格式化、图片识别与本轮向量召回
        middlewares.emplace_back(std::make_unique<FormatMessageMiddleware>());
        // 写入用户或助手消息记录，并发布消息已记录事件
        middlewares.emplace_back(std::make_unique<RecordMessageMiddleware>());
        // 命令先持久化，再执行并终止后续常规流程
        middlewares.emplace_back(std::make_unique<CommandExecutionMiddleware>());
        // 调用 Agent 处理消息，并按决策发送文本回复
        middlewares.emplace_back(std::make_unique<AgentReplyMiddleware>());
        // Agent 完成后发布消息主处理事件，触发统计和记忆维护
        middlewares.emplace_back(std::make_unique<PostProcessMiddleware>());

        return middlewares;
    }
} // namespace insoulforge
