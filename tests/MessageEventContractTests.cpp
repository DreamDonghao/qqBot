/// @file MessageEventContractTests.cpp
/// @brief 消息链路与领域事件的契约测试

#include <cstddef>
#include <config/Config.hpp>
#include <drogon/utils/coroutine.h>
#include <event/EventBus.hpp>
#include <event/subscribers/MemoryMaintenanceSubscriber.hpp>
#include <event/subscribers/MessageWebSocketSubscriber.hpp>
#include <event/subscribers/SessionStatisticsSubscriber.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <message/MessageMiddleware.hpp>
#include <message/MessageContext.hpp>
#include <message/MessagePipeline.hpp>
#include <message/middleware/AgentAvailabilityMiddleware.hpp>
#include <message/middleware/EventNormalizationMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <model/OneBotMessage.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    int failures = 0;

    void check(const bool condition, const std::string_view expression, const std::string_view testName) {
        if (!condition) {
            std::cerr << "[FAIL] " << testName << ": " << expression << '\n';
            ++failures;
        }
    }

    /// @brief 用于验证管线依赖注入的无副作用消息运行时
    class TestMessageRuntime final : public insoulforge::MessageRuntime {
    public:
        explicit TestMessageRuntime(const bool agentRunning) : m_agentRunning(agentRunning) {}

        [[nodiscard]] bool isAgentRunning() const override {
            ++m_availabilityChecks;
            return m_agentRunning;
        }

        drogon::Task<std::optional<std::string>> processAgent(insoulforge::ChatRecordManager &,
          insoulforge::MemoryManager &, const insoulforge::OneBotMessage &) const override {
            co_return std::nullopt;
        }

        drogon::Task<> sendReply(
          const insoulforge::OneBotMessage &, const insoulforge::ChatRecordManager &, std::string) const override {
            co_return;
        }

        drogon::Task<> publish(insoulforge::DomainEvent) const override { co_return; }

        [[nodiscard]] size_t availabilityChecks() const { return m_availabilityChecks; }

    private:
        bool m_agentRunning;
        mutable size_t m_availabilityChecks{0};
    };

    class RecordingMiddleware final : public insoulforge::MessageMiddleware {
    public:
        RecordingMiddleware(std::string id, std::vector<std::string> &trace,
          const insoulforge::MessageFlow flow = insoulforge::MessageFlow::Continue, const bool throws = false) :
            m_id(std::move(id)), m_trace(trace), m_flow(flow), m_throws(throws) {}

        [[nodiscard]] std::string_view id() const noexcept override { return m_id; }

        drogon::Task<insoulforge::MessageFlow> handle(insoulforge::MessageContext &) const override {
            m_trace.push_back(m_id);
            if (m_throws) {
                throw std::runtime_error("expected middleware failure");
            }
            co_return m_flow;
        }

    private:
        std::string m_id;
        std::vector<std::string> &m_trace;
        insoulforge::MessageFlow m_flow;
        bool m_throws;
    };

    [[nodiscard]] std::unique_ptr<RecordingMiddleware> recordingMiddleware(std::string id,
      std::vector<std::string> &trace, const insoulforge::MessageFlow flow = insoulforge::MessageFlow::Continue,
      const bool throws = false) {
        return std::make_unique<RecordingMiddleware>(std::move(id), trace, flow, throws);
    }

    void testMiddlewareInsertionOrder() {
        constexpr std::string_view kTestName = "middleware insertion order";
        std::vector<std::string> trace;
        insoulforge::MessagePipeline pipeline;
        std::vector<std::unique_ptr<insoulforge::MessageMiddleware>> middlewares;
        middlewares.push_back(recordingMiddleware("first", trace));
        middlewares.push_back(recordingMiddleware("anchor", trace));
        middlewares.push_back(recordingMiddleware("last", trace));
        pipeline.initialize(std::make_shared<TestMessageRuntime>(true), std::move(middlewares));
        pipeline.insertBefore("anchor", recordingMiddleware("before", trace));
        pipeline.insertAfter("anchor", recordingMiddleware("after", trace));

        drogon::sync_wait(pipeline.process(insoulforge::json::object()));
        check(trace == std::vector<std::string>({"first", "before", "anchor", "after", "last"}), "execution order",
          kTestName);
    }

    void testMiddlewareStopShortCircuits() {
        constexpr std::string_view kTestName = "middleware stop short circuits";
        std::vector<std::string> trace;
        insoulforge::MessagePipeline pipeline;
        std::vector<std::unique_ptr<insoulforge::MessageMiddleware>> middlewares;
        middlewares.push_back(recordingMiddleware("first", trace));
        middlewares.push_back(recordingMiddleware("stop", trace, insoulforge::MessageFlow::Stop));
        middlewares.push_back(recordingMiddleware("last", trace));
        pipeline.initialize(std::make_shared<TestMessageRuntime>(true), std::move(middlewares));

        drogon::sync_wait(pipeline.process(insoulforge::json::object()));
        check(trace == std::vector<std::string>({"first", "stop"}), "short circuit trace", kTestName);
    }

    void testMiddlewareExceptionStopsChain() {
        constexpr std::string_view kTestName = "middleware exception stops chain";
        std::vector<std::string> trace;
        insoulforge::MessagePipeline pipeline;
        std::vector<std::unique_ptr<insoulforge::MessageMiddleware>> middlewares;
        middlewares.push_back(recordingMiddleware("first", trace));
        middlewares.push_back(recordingMiddleware("failing", trace, insoulforge::MessageFlow::Continue, true));
        middlewares.push_back(recordingMiddleware("last", trace));
        pipeline.initialize(std::make_shared<TestMessageRuntime>(true), std::move(middlewares));

        drogon::sync_wait(pipeline.process(insoulforge::json::object()));
        check(trace == std::vector<std::string>({"first", "failing"}), "exception isolation trace", kTestName);
    }

    void testPipelineUsesInjectedRuntime() {
        constexpr std::string_view kTestName = "pipeline uses injected runtime";
        std::vector<std::string> trace;
        auto runtime = std::make_shared<TestMessageRuntime>(false);
        insoulforge::MessagePipeline pipeline;
        std::vector<std::unique_ptr<insoulforge::MessageMiddleware>> middlewares;
        middlewares.push_back(std::make_unique<insoulforge::AgentAvailabilityMiddleware>());
        middlewares.push_back(recordingMiddleware("after_agent_check", trace));
        pipeline.initialize(runtime, std::move(middlewares));

        drogon::sync_wait(pipeline.process(insoulforge::json::object()));
        check(runtime->availabilityChecks() == 1, "runtime availability check count", kTestName);
        check(trace.empty(), "later middleware was short circuited", kTestName);
    }

    void testRichMessageContentKeepsNonTextSegmentsOutOfText() {
        constexpr std::string_view kTestName = "rich message content segments";
        auto &config = insoulforge::Config::instance();
        const uint64_t originalSelfId = config.selfQQNumber;
        config.selfQQNumber = 42;
        insoulforge::OneBotMessage::setCustomQQName(11, "Alice");
        insoulforge::OneBotMessage::setCustomQQName(42, "Bot");

        insoulforge::json event;
        event["post_type"] = "message";
        event["message_type"] = "group";
        event["self_id"] = 42;
        event["group_id"] = 100;
        event["message_id"] = 7;
        event["sender"] = {{"user_id", 11}, {"nickname", "Alice"}};
        event["message"] = {{{"type", "face"}, {"data", {{"id", "178"}, {"raw", {{"faceText", "笑脸"}}}}}},
          {{"type", "poke"}, {"data", {{"actor_id", 11}, {"target_id", 42}}}}};

        insoulforge::OneBotMessage message(std::move(event));
        drogon::sync_wait(message.enrichContent());
        const insoulforge::json record = insoulforge::parseJson(message.recordContent());
        check(!record.contains("text"), "non-text segments do not populate text", kTestName);
        check(record["faces"].size() == 1, "face is stored independently", kTestName);
        check(record["notifications"].size() == 1, "poke is stored independently", kTestName);
        check(message.hasFace(), "face marker", kTestName);
        check(message.isPokeForBot(), "bot poke marker", kTestName);

        config.selfQQNumber = originalSelfId;
    }

    void testMembershipNoticeNormalizesToRichNotification() {
        constexpr std::string_view kTestName = "membership notice normalization";
        auto runtime = std::make_shared<TestMessageRuntime>(true);
        insoulforge::json notice = {{"post_type", "notice"},
          {"notice_type", "group_increase"},
          {"group_id", 100},
          {"user_id", 11},
          {"operator_id", 12}};
        insoulforge::MessageContext context(std::move(notice), runtime);
        insoulforge::EventNormalizationMiddleware middleware;

        const auto flow = drogon::sync_wait(middleware.handle(context));
        check(flow == insoulforge::MessageFlow::Continue, "supported notice continues", kTestName);
        context.createMessage();
        check(context.message().hasMembershipNotification(), "membership marker", kTestName);
        drogon::sync_wait(context.message().enrichContent());
        const insoulforge::json record = insoulforge::parseJson(context.message().recordContent());
        check(!record.contains("text"), "notification does not populate text", kTestName);
        check(record["notifications"][0]["kind"] == "member_join", "join kind", kTestName);
    }

    void testPokeNoticeNormalizesToRichNotification() {
        constexpr std::string_view kTestName = "poke notice normalization";
        auto &config = insoulforge::Config::instance();
        const uint64_t originalSelfId = config.selfQQNumber;
        config.selfQQNumber = 42;
        insoulforge::OneBotMessage::setCustomQQName(11, "Alice");
        insoulforge::OneBotMessage::setCustomQQName(42, "Bot");

        auto runtime = std::make_shared<TestMessageRuntime>(true);
        insoulforge::json notice = {{"post_type", "notice"},
          {"notice_type", "notify"},
          {"sub_type", "poke"},
          {"group_id", 100},
          {"user_id", 11},
          {"target_id", 42}};
        insoulforge::MessageContext context(std::move(notice), runtime);
        insoulforge::EventNormalizationMiddleware middleware;

        const auto flow = drogon::sync_wait(middleware.handle(context));
        check(flow == insoulforge::MessageFlow::Continue, "supported notice continues", kTestName);
        context.createMessage();
        check(context.message().isPokeForBot(), "bot poke marker", kTestName);
        drogon::sync_wait(context.message().enrichContent());
        const insoulforge::json record = insoulforge::parseJson(context.message().recordContent());
        check(!record.contains("text"), "poke does not populate text", kTestName);
        check(record["notifications"][0]["type"] == "poke", "poke type", kTestName);

        config.selfQQNumber = originalSelfId;
    }

    void testEventSubscriberExceptionDoesNotStopDispatch() {
        constexpr std::string_view kTestName = "event subscriber exception isolation";
        std::vector<std::string> trace;
        insoulforge::EventBus eventBus;
        eventBus.initialize([&trace](insoulforge::EventBus &bus) {
            bus.subscribe<insoulforge::MessageRecordedEvent>(
              "first", [&trace](const insoulforge::MessageRecordedEvent &) -> drogon::Task<> {
                  trace.push_back("first");
                  co_return;
              });
            bus.subscribe<insoulforge::MessageRecordedEvent>(
              "failing", [&trace](const insoulforge::MessageRecordedEvent &) -> drogon::Task<> {
                  trace.push_back("failing");
                  throw std::runtime_error("expected subscriber failure");
                  co_return;
              });
            bus.subscribe<insoulforge::MessageRecordedEvent>(
              "last", [&trace](const insoulforge::MessageRecordedEvent &) -> drogon::Task<> {
                  trace.push_back("last");
                  co_return;
              });
        });

        drogon::sync_wait(eventBus.publish(insoulforge::MessageRecordedEvent{
          .sessionId = 42,
          .messageId = 7,
          .role = insoulforge::MessageRole::User,
          .recordContent = "record",
          .displayContent = "display",
        }));
        check(trace == std::vector<std::string>({"first", "failing", "last"}), "subscriber dispatch trace", kTestName);
    }

    void testEventSubscribersUseInjectedDependencies() {
        constexpr std::string_view kTestName = "event subscribers use injected dependencies";
        std::vector<std::string> trace;
        insoulforge::EventBus eventBus;
        eventBus.initialize([&trace](insoulforge::EventBus &bus) {
            insoulforge::MessageWebSocketSubscriber messageWebSocketSubscriber(
              [&trace](const insoulforge::MessageRecordedEvent &event) {
                  trace.push_back("push:" + std::to_string(event.sessionId) + ":" + event.displayContent);
              });
            insoulforge::SessionStatisticsSubscriber sessionStatisticsSubscriber(
              [&trace](const insoulforge::MessageProcessingCompletedEvent &event) {
                  trace.push_back("statistics:" + std::to_string(event.contentSize));
              });
            insoulforge::MemoryMaintenanceSubscriber memoryMaintenanceSubscriber(
              [&trace](const insoulforge::MessageProcessingCompletedEvent &event) -> drogon::Task<> {
                  trace.push_back("memory:" + std::to_string(event.sessionId));
                  co_return;
              });

            messageWebSocketSubscriber.registerHandlers(bus);
            sessionStatisticsSubscriber.registerHandlers(bus);
            memoryMaintenanceSubscriber.registerHandlers(bus);
        });

        drogon::sync_wait(eventBus.publish(insoulforge::MessageRecordedEvent{
          .sessionId = 42,
          .messageId = 7,
          .role = insoulforge::MessageRole::User,
          .recordContent = "record",
          .displayContent = "display",
        }));
        drogon::sync_wait(eventBus.publish(insoulforge::MessageProcessingCompletedEvent{
          .sessionId = 42,
          .messageId = 7,
          .contentSize = 12,
        }));

        check(trace == std::vector<std::string>({"push:42:display", "statistics:12", "memory:42"}),
          "injected side effect trace", kTestName);
    }
} // namespace

int main() {
    testMiddlewareInsertionOrder();
    testMiddlewareStopShortCircuits();
    testMiddlewareExceptionStopsChain();
    testPipelineUsesInjectedRuntime();
    testRichMessageContentKeepsNonTextSegmentsOutOfText();
    testMembershipNoticeNormalizesToRichNotification();
    testPokeNoticeNormalizesToRichNotification();
    testEventSubscriberExceptionDoesNotStopDispatch();
    testEventSubscribersUseInjectedDependencies();

    if (failures == 0) {
        std::cout << "All message and event contract tests passed\n";
        return 0;
    }
    std::cerr << failures << " contract test(s) failed\n";
    return 1;
}
