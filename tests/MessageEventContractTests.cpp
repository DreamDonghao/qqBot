/// @file MessageEventContractTests.cpp
/// @brief 消息链路与领域事件的契约测试

#include <config/Config.hpp>
#include <cstddef>
#include <drogon/utils/coroutine.h>
#include <event/EventBus.hpp>
#include <event/subscribers/MemoryMaintenanceSubscriber.hpp>
#include <event/subscribers/MessageWebSocketSubscriber.hpp>
#include <event/subscribers/SessionStatisticsSubscriber.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <message/MessageContext.hpp>
#include <message/MessageMiddleware.hpp>
#include <message/MessagePipeline.hpp>
#include <message/MessageRecord.hpp>
#include <message/middleware/AgentAvailabilityMiddleware.hpp>
#include <message/middleware/EventNormalizationMiddleware.hpp>
#include <message/runtime/MessageRuntime.hpp>
#include <message/workflow/AgentResponseWorkflow.hpp>
#include <model/OneBotMessage.hpp>
#include <stdexcept>
#include <storage/ConfigStore.hpp>
#include <storage/Database.hpp>
#include <storage/ImageDescriptionStore.hpp>
#include <storage/SchemaMigrator.hpp>
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

        drogon::Task<insoulforge::AgentProcessResult> processAgent(insoulforge::ChatRecordManager &,
          insoulforge::MemoryManager &, const insoulforge::OneBotMessage &) const override {
            co_return {};
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

    /// @brief 用于验证 Agent 决策与回复投递分支的消息运行时
    class WorkflowMessageRuntime final : public insoulforge::MessageRuntime {
    public:
        explicit WorkflowMessageRuntime(insoulforge::AgentProcessResult result, const bool failToSend = false) :
            m_result(std::move(result)), m_failToSend(failToSend) {}

        [[nodiscard]] bool isAgentRunning() const override { return true; }

        drogon::Task<insoulforge::AgentProcessResult> processAgent(insoulforge::ChatRecordManager &,
          insoulforge::MemoryManager &, const insoulforge::OneBotMessage &) const override {
            co_return m_result;
        }

        drogon::Task<> sendReply(const insoulforge::OneBotMessage &, const insoulforge::ChatRecordManager &,
          std::string content) const override {
            if (m_failToSend) {
                throw std::runtime_error("expected send failure");
            }
            sentReplies.push_back(std::move(content));
            co_return;
        }

        drogon::Task<> publish(insoulforge::DomainEvent) const override { co_return; }

        mutable std::vector<std::string> sentReplies;

    private:
        insoulforge::AgentProcessResult m_result;
        bool m_failToSend;
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

    [[nodiscard]] insoulforge::OneBotMessage createWorkflowMessage() {
        return insoulforge::OneBotMessage({{"post_type", "message"}, {"message_type", "group"}, {"self_id", 42},
          {"group_id", 100}, {"message_id", 7}, {"sender", {{"user_id", 11}, {"nickname", "Alice"}}},
          {"message", {{{"type", "text"}, {"data", {{"text", "hello"}}}}}}});
    }

    void testAgentResponseWorkflowRoutesOutcomes() {
        constexpr std::string_view kTestName = "agent response workflow routes outcomes";
        auto replyRuntime = std::make_shared<WorkflowMessageRuntime>(
          insoulforge::AgentProcessResult{.outcome = insoulforge::AgentProcessResult::Outcome::Reply, .content = "hi"});
        const auto replyOutcome =
          drogon::sync_wait(insoulforge::AgentResponseWorkflow::execute(replyRuntime, createWorkflowMessage(), {}));
        check(
          replyOutcome == insoulforge::MessageProcessingOutcome::ReplySent, "reply branch sends message", kTestName);
        check(replyRuntime->sentReplies == std::vector<std::string>({"hi"}), "reply content is delivered", kTestName);

        auto skipRuntime = std::make_shared<WorkflowMessageRuntime>(
          insoulforge::AgentProcessResult{.outcome = insoulforge::AgentProcessResult::Outcome::Cancelled});
        const auto skipOutcome =
          drogon::sync_wait(insoulforge::AgentResponseWorkflow::execute(skipRuntime, createWorkflowMessage(), {}));
        check(skipOutcome == insoulforge::MessageProcessingOutcome::AgentCancelled, "cancelled branch skips delivery",
          kTestName);
        check(skipRuntime->sentReplies.empty(), "cancelled branch has no reply", kTestName);

        auto failedRuntime = std::make_shared<WorkflowMessageRuntime>(
          insoulforge::AgentProcessResult{.outcome = insoulforge::AgentProcessResult::Outcome::Reply, .content = "hi"},
          true);
        const auto failedOutcome =
          drogon::sync_wait(insoulforge::AgentResponseWorkflow::execute(failedRuntime, createWorkflowMessage(), {}));
        check(failedOutcome == insoulforge::MessageProcessingOutcome::ReplyFailed, "send failure has distinct outcome",
          kTestName);
    }

    void testRichMessageContentUsesCanonicalSegments() {
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
        check(!record.contains("faces"), "face has no duplicate collection", kTestName);
        check(!record.contains("notifications"), "poke has no duplicate collection", kTestName);
        check(record["segments"][0]["type"] == "face", "face is stored in ordered segments", kTestName);
        check(record["segments"][1]["type"] == "poke", "poke is stored in ordered segments", kTestName);
        check(message.hasFace(), "face marker", kTestName);
        check(message.isPokeForBot(), "bot poke marker", kTestName);

        config.selfQQNumber = originalSelfId;
    }

    void testMembershipNoticeNormalizesToRichNotification() {
        constexpr std::string_view kTestName = "membership notice normalization";
        auto runtime = std::make_shared<TestMessageRuntime>(true);
        insoulforge::json notice = {{"post_type", "notice"}, {"notice_type", "group_increase"}, {"group_id", 100},
          {"user_id", 11}, {"operator_id", 12}};
        insoulforge::MessageContext context(std::move(notice), runtime);
        insoulforge::EventNormalizationMiddleware middleware;

        const auto flow = drogon::sync_wait(middleware.handle(context));
        check(flow == insoulforge::MessageFlow::Continue, "supported notice continues", kTestName);
        context.createMessage();
        check(context.message().hasMembershipNotification(), "membership marker", kTestName);
        drogon::sync_wait(context.message().enrichContent());
        const insoulforge::json record = insoulforge::parseJson(context.message().recordContent());
        check(!record.contains("text"), "notification does not populate text", kTestName);
        check(record["segments"][0]["type"] == "member_event", "membership is an ordered segment", kTestName);
        check(record["segments"][0]["action"] == "join", "join action", kTestName);
    }

    void testPokeNoticeNormalizesToRichNotification() {
        constexpr std::string_view kTestName = "poke notice normalization";
        auto &config = insoulforge::Config::instance();
        const uint64_t originalSelfId = config.selfQQNumber;
        config.selfQQNumber = 42;
        insoulforge::OneBotMessage::setCustomQQName(11, "Alice");
        insoulforge::OneBotMessage::setCustomQQName(42, "Bot");

        auto runtime = std::make_shared<TestMessageRuntime>(true);
        insoulforge::json notice = {{"post_type", "notice"}, {"notice_type", "notify"}, {"sub_type", "poke"},
          {"group_id", 100}, {"user_id", 11}, {"target_id", 42}};
        insoulforge::MessageContext context(std::move(notice), runtime);
        insoulforge::EventNormalizationMiddleware middleware;

        const auto flow = drogon::sync_wait(middleware.handle(context));
        check(flow == insoulforge::MessageFlow::Continue, "supported notice continues", kTestName);
        context.createMessage();
        check(context.message().isPokeForBot(), "bot poke marker", kTestName);
        drogon::sync_wait(context.message().enrichContent());
        const insoulforge::json record = insoulforge::parseJson(context.message().recordContent());
        check(!record.contains("text"), "poke does not populate text", kTestName);
        check(record["segments"][0]["type"] == "poke", "poke type", kTestName);

        config.selfQQNumber = originalSelfId;
    }

    void testMessageRecordProjectionHidesImageSources() {
        constexpr std::string_view kTestName = "message record projection";
        const insoulforge::json record = {
          {"time", "2026-09-07 17:22:05"},
          {"sender", {{"name", "Alice"}, {"qq", "11"}}},
          {"message_id", "7"},
          {"segments", {{{"type", "text"}, {"text", "看这张图"}}, {{"type", "image"}, {"image_index", 0}}}},
          {"assets", {{"images", {{{"recognition_status", "succeeded"}, {"description", "一只蓝色的猫"},
                                   {"source", {{"file", "cat.jpg"}, {"url", "https://example.com/cat.jpg"}}}}}}}},
        };

        const insoulforge::json projected = insoulforge::MessageRecord::projectForAgent(record);
        check(!projected.contains("assets"), "agent projection excludes assets", kTestName);
        check(projected["segments"][1]["image_index"] == 0, "image index remains stable", kTestName);
        check(
          projected["segments"][1]["description"] == "一只蓝色的猫", "image description remains visible", kTestName);
        check(
          !projected.dump().contains("https://example.com/cat.jpg"), "agent projection excludes image URL", kTestName);

        const auto source = insoulforge::MessageRecord::findImageSource(record, 0);
        check(source && source->file == "cat.jpg", "tool can resolve image source", kTestName);
        check(insoulforge::MessageRecord::extractRecallText(record).contains("一只蓝色的猫"),
          "image description participates in recall", kTestName);

        const insoulforge::json legacyRecord = {
          {"message_id", "8"},
          {"segments", {{{"type", "image"}, {"file", "legacy.jpg"}, {"url", "https://example.com/legacy.jpg"},
                         {"recognition_status", "succeeded"}, {"description", "旧版图片"}}}},
          {"images", {{{"file", "legacy.jpg"}, {"url", "https://example.com/legacy.jpg"},
                       {"recognition_status", "succeeded"}, {"description", "旧版图片"}}}},
        };
        const insoulforge::json legacyProjection = insoulforge::MessageRecord::projectForAgent(legacyRecord);
        check(!legacyProjection.dump().contains("https://example.com/legacy.jpg"),
          "legacy projection excludes image URL", kTestName);
        const auto legacySource = insoulforge::MessageRecord::findImageSource(legacyRecord, 0);
        check(legacySource && legacySource->file == "legacy.jpg", "tool resolves legacy image source", kTestName);
    }

    void testAssistantStickerRecordKeepsOnlyName() {
        constexpr std::string_view kTestName = "assistant sticker record";
        const insoulforge::json record = insoulforge::MessageRecord::createAssistantRecord(
          "Bot(我)", 7, "[CQ:image,file=https://example.com/sticker.jpg,sub_type=1,summary=嘲讽]");
        check(record["segments"].size() == 1, "sticker has one semantic segment", kTestName);
        check(record["segments"][0]["type"] == "sticker", "sticker segment type", kTestName);
        check(record["segments"][0]["name"] == "嘲讽", "sticker summary becomes name", kTestName);
        check(
          !record.dump().contains("https://example.com/sticker.jpg"), "sticker record excludes CQ source", kTestName);
    }

    void testImageDescriptionCacheSchemaMigration() {
        constexpr std::string_view kTestName = "image description cache schema migration";
        sqlite3 *db = nullptr;
        check(sqlite3_open(":memory:", &db) == SQLITE_OK, "opens in-memory database", kTestName);
        if (!db)
            return;

        sqlite3_exec(db, "PRAGMA user_version = 6", nullptr, nullptr, nullptr);
        insoulforge::SchemaMigrator::migrate(db);
        sqlite3_stmt *stmt = nullptr;
        const int prepareResult =
          sqlite3_prepare_v2(db, "SELECT sampled_frame_count FROM image_description_cache LIMIT 1", -1, &stmt, nullptr);
        check(prepareResult == SQLITE_OK, "v6 migration creates sampled frame count column", kTestName);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void testImageDescriptionCacheStore() {
        constexpr std::string_view kTestName = "image description cache store";
        auto &database = insoulforge::Database::instance();
        database.initialize(":memory:");

        insoulforge::ImageDescriptionStore::upsert("hash", "vision-model", 1, "gif", true, "角色挥手", 16);
        const auto succeeded = insoulforge::ImageDescriptionStore::find("hash", "vision-model", 1);
        check(succeeded && succeeded->succeeded, "stores successful description", kTestName);
        check(succeeded && succeeded->description == "角色挥手", "stores description", kTestName);
        check(succeeded && succeeded->sampledFrameCount == 16, "stores sampled frame count", kTestName);

        insoulforge::ImageDescriptionStore::upsert("hash", "vision-model", 1, "gif", false, "", 0);
        const auto failed = insoulforge::ImageDescriptionStore::find("hash", "vision-model", 1);
        check(failed && !failed->succeeded, "updates cached failure", kTestName);
        check(insoulforge::ImageDescriptionStore::clearAll() == 1, "clears all cached descriptions", kTestName);
        check(!insoulforge::ImageDescriptionStore::find("hash", "vision-model", 1), "cleared entry is unavailable",
          kTestName);

        insoulforge::ConfigStore::saveLLMConfig(
          "image", {{"apiKey", "key"}, {"baseUrl", "https://example.com"}, {"path", "/v1/chat/completions"},
                     {"model", "vision-model"}, {"maxTokens", 1536}, {"temperature", 0.4}, {"topP", 0.8},
                     {"reasoningEffort", ""}});
        insoulforge::Config::instance().loadFromDatabase();
        check(insoulforge::Config::instance().imageParams.maxTokens == 1536, "loads configured image max tokens",
          kTestName);
        database.close();
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
          .outcome = insoulforge::MessageProcessingOutcome::ReplySent,
        }));

        check(trace == std::vector<std::string>({"push:42:display", "statistics:12", "memory:42"}),
          "injected side effect trace", kTestName);

        drogon::sync_wait(eventBus.publish(insoulforge::MessageProcessingCompletedEvent{
          .sessionId = 42,
          .messageId = 8,
          .contentSize = 8,
          .outcome = insoulforge::MessageProcessingOutcome::CommandHandled,
        }));
        check(trace == std::vector<std::string>({"push:42:display", "statistics:12", "memory:42"}),
          "commands do not trigger conversation side effects", kTestName);
    }
} // namespace

int main() {
    testMiddlewareInsertionOrder();
    testMiddlewareStopShortCircuits();
    testMiddlewareExceptionStopsChain();
    testPipelineUsesInjectedRuntime();
    testAgentResponseWorkflowRoutesOutcomes();
    testRichMessageContentUsesCanonicalSegments();
    testMembershipNoticeNormalizesToRichNotification();
    testPokeNoticeNormalizesToRichNotification();
    testMessageRecordProjectionHidesImageSources();
    testAssistantStickerRecordKeepsOnlyName();
    testImageDescriptionCacheSchemaMigration();
    testImageDescriptionCacheStore();
    testEventSubscriberExceptionDoesNotStopDispatch();
    testEventSubscribersUseInjectedDependencies();

    if (failures == 0) {
        std::cout << "All message and event contract tests passed\n";
        return 0;
    }
    std::cerr << failures << " contract test(s) failed\n";
    return 1;
}
