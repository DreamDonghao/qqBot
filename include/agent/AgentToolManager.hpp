/// @file AgentToolManager.hpp
/// @brief Agent 工具管理器 - 注册所有可用工具
/// @author donghao
/// @date 2026-04-02
/// @details 负责注册和管理 Agent 可使用的工具：
///          - 终端工具：no_reply, reply
///          - 信息工具：get_weather, search_web, get_time, search_knowledge, recall_memory
///          - 动作工具：random, send_face, send_image, send_emoji, at_user, ban_user
///          - 自定义工具：从数据库加载用户定义的工具

#pragma once
#include <service/ToolRegistry.hpp>
#include <service/RAGFlowClient.hpp>
#include <api/ApiClient.hpp>
#include <storage/Database.hpp>
#include <drogon/utils/coroutine.h>
#include <drogon/HttpClient.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <string>
#include <unordered_map>
#include <random>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace LittleMeowBot {
    // 工具限制常量
    static constexpr int MAX_RANDOM_COUNT = 10;

    /// @brief 工具管理器 - 注册所有可用工具
    class AgentToolManager {
    public:
        static AgentToolManager& instance() {
            static AgentToolManager manager;
            return manager;
        }

        /// @brief 注册所有工具
        void registerAllTools() const{
            auto& registry = ToolRegistry::instance();

            // ========== 终端工具 ==========

            // no_reply
            registry.registerTool(
                {
                    .name = "no_reply",
                    .description = "决定不回复消息。当：话题已参与过、没人问你、刚说过话、纯表情刷屏时使用。",
                    .parameters = Json::Value(),
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> { co_return "ok"; }
                }, ToolCategory::TERMINAL
            );

            // reply
            Json::Value replyParams;
            replyParams["type"] = "object";
            replyParams["properties"]["content"]["type"] = "string";
            replyParams["properties"]["content"]["description"] = "要发送的回复内容";
            replyParams["required"].append("content");
            registry.registerTool(
                {
                    .name = "reply",
                    .description = "回复消息。当：有人开启新的话题、有人问你、有人@你、有人求助时使用。",
                    .parameters = replyParams,
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> { co_return "ok"; }
                }, ToolCategory::TERMINAL
            );

            // ========== 信息工具 ==========

            // get_weather
            Json::Value weatherParams;
            weatherParams["type"] = "object";
            weatherParams["properties"]["city"]["type"] = "string";
            weatherParams["properties"]["city"]["description"] = "城市名称，如：北京、上海、广州";
            weatherParams["required"].append("city");
            registry.registerTool(
                {
                    .name = "get_weather",
                    .description = "查询指定城市的天气信息。当有人问天气时使用。",
                    .parameters = weatherParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        std::string city = args.isMember("city") ? args["city"].asString() : "北京";
                        co_return co_await ApiClient::fetchWeather(city);
                    }
                }, ToolCategory::INFORMATION
            );

            // search_web
            Json::Value searchParams;
            searchParams["type"] = "object";
            searchParams["properties"]["query"]["type"] = "string";
            searchParams["properties"]["query"]["description"] = "搜索关键词";
            searchParams["required"].append("query");
            registry.registerTool(
                {
                    .name = "search_web",
                    .description = "搜索网络获取信息。当需要查询实时信息、新闻、知识、最新动态时使用。",
                    .parameters = searchParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        std::string query = args.isMember("query") ? args["query"].asString() : "";
                        co_return co_await ApiClient::searchWeb(query);
                    }
                }, ToolCategory::INFORMATION
            );

            // get_time
            Json::Value timeParams;
            timeParams["type"] = "object";
            timeParams["properties"]["timezone"]["type"] = "string";
            timeParams["properties"]["timezone"]["description"] = "时区，默认使用系统时区";
            registry.registerTool(
                {
                    .name = "get_time",
                    .description = "获取当前日期和时间。当有人问现在几点、今天星期几、什么日期时使用。",
                    .parameters = timeParams,
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> {
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);

                        std::ostringstream oss;
                        oss << std::put_time(std::localtime(&time), "%Y年%m月%d日 %A %H:%M:%S");

                        std::string result = oss.str();
                        static const std::unordered_map<std::string, std::string> weekdayMap = {
                            {"Monday", "星期一"}, {"Tuesday", "星期二"}, {"Wednesday", "星期三"},
                            {"Thursday", "星期四"}, {"Friday", "星期五"}, {"Saturday", "星期六"},
                            {"Sunday", "星期日"}
                        };
                        for (const auto& [en, zh] : weekdayMap) {
                            size_t pos = result.find(en);
                            if (pos != std::string::npos) {
                                result.replace(pos, en.length(), zh);
                                break;
                            }
                        }
                        co_return "现在时间：" + result;
                    }
                }, ToolCategory::INFORMATION
            );

            // list_emojis
            registry.registerTool(
                {
                    .name = "list_emojis",
                    .description = "获取本地表情库中所有可用的表情名称列表。",
                    .parameters = Json::Value(),
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> {
                        auto& db = Database::instance();
                        auto emojis = db.getAllEmojis();
                        std::string result = "可用表情: ";
                        bool first = true;
                        for (const auto& name : emojis | std::views::keys) {
                            if (!first) result += ", ";
                            result += name;
                            first = false;
                        }
                        co_return emojis.empty() ? "表情库为空" : result;
                    }
                }, ToolCategory::INFORMATION
            );

            // search_knowledge
            Json::Value knowledgeParams;
            knowledgeParams["type"] = "object";
            knowledgeParams["properties"]["query"]["type"] = "string";
            knowledgeParams["properties"]["query"]["description"] = "检索问题";
            knowledgeParams["required"].append("query");
            registry.registerTool(
                {
                    .name = "search_knowledge",
                    .description = "从知识库中检索信息。当用户问FAQ、专业知识时使用。",
                    .parameters = knowledgeParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        const std::string query = args.isMember("query") ? args["query"].asString() : "";
                        if (query.empty()) co_return std::string("请提供检索问题");

                        const auto result = co_await RAGFlowClient::instance().searchKnowledge(query);
                        co_return result.value_or("知识库检索失败");
                    }
                }, ToolCategory::INFORMATION
            );

            // recall_memory
            Json::Value memoryParams;
            memoryParams["type"] = "object";
            memoryParams["properties"]["query"]["type"] = "string";
            memoryParams["properties"]["query"]["description"] = "要回忆的内容关键词，如某人的喜好、某群的习惯等";
            memoryParams["required"].append("query");
            registry.registerTool(
                {
                    .name = "recall_memory",
                    .description = "从长期记忆库中回忆信息（如果需要可以先获取当前群聊的名称）。当想不起某人喜好、某群习惯、过去的约定时使用。模拟人类回忆过程。",
                    .parameters = memoryParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        const std::string query = args.isMember("query") ? args["query"].asString() : "";
                        if (query.empty()) co_return std::string("请提供回忆关键词");

                        const auto result = co_await RAGFlowClient::instance().searchMemory(query);
                        if (!result || result->empty()) {
                            co_return "想不起来了，没有找到相关记忆";
                        }
                        co_return "回忆起：" + result.value();
                    }
                }, ToolCategory::INFORMATION
            );

            // get_group_name
            registry.registerTool(
                {
                    .name = "get_group_name",
                    .description = "获取当前群聊的名称。当需要知道群名或确认当前群时使用。",
                    .parameters = Json::Value(),
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> {
                        const auto& ctx = currentToolContext();
                        if (ctx.groupId == 0) {
                            co_return "无法获取群信息";
                        }
                        if (!ctx.groupName.empty()) {
                            co_return fmt::format("当前群：{}（群号：{}）", ctx.groupName, ctx.groupId);
                        }
                        co_return fmt::format("当前群号：{}", ctx.groupId);
                    }
                }, ToolCategory::INFORMATION
            );

            // ========== 动作工具 ==========

            // random
            Json::Value randomParams;
            randomParams["type"] = "object";
            randomParams["properties"]["min"]["type"] = "integer";
            randomParams["properties"]["min"]["description"] = "最小值，默认1";
            randomParams["properties"]["max"]["type"] = "integer";
            randomParams["properties"]["max"]["description"] = "最大值，默认100";
            randomParams["properties"]["count"]["type"] = "integer";
            randomParams["properties"]["count"]["description"] = "生成数量，默认1";
            randomParams["properties"]["description"]["type"] = "string";
            randomParams["properties"]["description"]["description"] = "描述这次随机的用途";
            registry.registerTool(
                {
                    .name = "random",
                    .description = "生成随机数。用于掷骰子、抽签、随机选择、碰运气等场景。",
                    .parameters = randomParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        int min = args.isMember("min") ? args["min"].asInt() : 1;
                        int max = args.isMember("max") ? args["max"].asInt() : 100;
                        int count = args.isMember("count") ? args["count"].asInt() : 1;
                        const std::string desc = args.isMember("description")
                                                     ? args["description"].asString()
                                                     : "随机";

                        if (min > max) std::swap(min, max);
                        if (count < 1) count = 1;
                        if (count > MAX_RANDOM_COUNT) count = MAX_RANDOM_COUNT;

                        std::string result = desc + "结果：";
                        thread_local std::mt19937 generator(std::random_device{}());
                        std::uniform_int_distribution<int> distribution(min, max);
                        for (int i = 0; i < count; i++) {
                            if (count > 1) result += "\n第" + std::to_string(i + 1) + "次: ";
                            result += std::to_string(distribution(generator));
                        }
                        co_return result;
                    }
                }, ToolCategory::ACTION
            );

            // send_face
            Json::Value faceParams;
            faceParams["type"] = "object";
            faceParams["properties"]["id"]["type"] = "integer";
            faceParams["properties"]["id"]["description"] =
                "表情ID，常用: 1-发呆, 2-撇嘴, 3-色, 4-发呆, 5-得意, 6-流泪, 7-害羞, 8-闭嘴, 9-睡, 10-大哭, 11-尴尬, 12-发怒, 13-调皮, 14-呲牙, 15-惊讶, 16-难过, 17-酷, 18-冷汗, 19-抓狂, 20-吐, 21-偷笑, 22-可爱, 23-白眼, 24-傲慢, 25-饥饿, 26-困, 27-惊恐, 28-流汗, 29-憨笑, 30-大兵, 31-奋斗, 32-咒骂, 33-疑问, 34-嘘, 35-晕, 36-折磨, 37-衰, 38-骷髅, 39-敲打, 40-再见";
            faceParams["required"].append("id");
            registry.registerTool(
                {
                    .name = "send_face",
                    .description = "发送QQ表情，不自主使用。",
                    .parameters = faceParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        int id = args.isMember("id") ? args["id"].asInt() : 1;
                        co_return fmt::format("[CQ:face,id={}]", id);
                    }
                }, ToolCategory::ACTION
            );

            // send_image
            Json::Value imageParams;
            imageParams["type"] = "object";
            imageParams["properties"]["url"]["type"] = "string";
            imageParams["properties"]["url"]["description"] = "图片URL地址";
            imageParams["required"].append("url");
            registry.registerTool(
                {
                    .name = "send_image",
                    .description = "发送图片。提供图片URL地址。用于分享图片、表情包等。",
                    .parameters = imageParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        std::string url = args.isMember("url") ? args["url"].asString() : "";
                        if (url.empty()) co_return std::string("请提供图片URL");
                        co_return fmt::format("[CQ:image,file={}]", url);
                    }
                }, ToolCategory::ACTION
            );

            // send_emoji
            Json::Value emojiParams;
            emojiParams["type"] = "object";
            emojiParams["properties"]["name"]["type"] = "string";
            emojiParams["properties"]["name"]["description"] = "表情名称，如: happy, sad, cat, dog 等";
            emojiParams["required"].append("name");
            registry.registerTool(
                {
                    .name = "send_emoji",
                    .description = "从本地表情库发送表情。常用表情名称会动态更新。返回CQ码用于reply。",
                    .parameters = emojiParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        std::string name = args.isMember("name") ? args["name"].asString() : "";
                        if (name.empty()) co_return std::string("请提供表情名称");

                        auto& db = Database::instance();
                        std::string path = db.getEmoji(name);
                        if (path.empty()) {
                            co_return fmt::format("表情'{}'不存在，可用表情请查询表情库", name);
                        }
                        co_return fmt::format("[CQ:image,file={}]", path);
                    }
                }, ToolCategory::ACTION
            );

            // at_user
            Json::Value atParams;
            atParams["type"] = "object";
            atParams["properties"]["qq"]["type"] = "string";
            atParams["properties"]["qq"]["description"] = "要@的QQ号（从聊天记录中看到的QQ号）。使用 'all' @全体成员";
            atParams["required"].append("qq");
            registry.registerTool(
                {
                    .name = "at_user",
                    .description = "@某人。返回CQ码嵌入reply中。例如聊天记录显示 '{小明[QQ:123456]}'，则用 at_user('123456') 来@他。@全体成员用 at_user('all')",
                    .parameters = atParams,
                    .handler = [](const Json::Value& args) -> drogon::Task<std::string> {
                        std::string qq = args.isMember("qq") ? args["qq"].asString() : "";
                        if (qq.empty()) co_return std::string("请提供QQ号");
                        if (qq == "all") {
                            co_return std::string("[CQ:at,qq=all]");
                        }
                        co_return fmt::format("[CQ:at,qq={}]", qq);
                    }
                }, ToolCategory::ACTION
            );

            // ban_user
            Json::Value banParams;
            banParams["type"] = "object";
            banParams["properties"]["qq"]["type"] = "string";
            banParams["properties"]["qq"]["description"] = "要禁言的QQ号";
            banParams["properties"]["duration"]["type"] = "integer";
            banParams["properties"]["duration"]["description"] = "禁言时长（秒）。轻度60-300秒，中度600-1800秒，重度3600秒以上。0解除禁言";
            banParams["required"].append("qq");
            registry.registerTool(
                {
                    .name = "ban_user",
                    .description = "禁言群成员。要有自己的判断，不要别人让你禁言就禁言。根据违规程度选择时长：轻度(偶尔骂人)60-300秒，中度(持续刷屏骂人)600-1800秒，重度(恶意骚扰)3600秒+",
                    .parameters = banParams,
                    .handler = [](const Json::Value&) -> drogon::Task<std::string> {
                        co_return "ban_user:需要异步执行"; // 实际执行在 ExecutorAgent 中
                    }
                }, ToolCategory::ACTION
            );

            spdlog::info("ToolManager: 工具注册完成（共13个工具）");
        }

        /// @brief 注册自定义工具（从数据库加载）
        void registerCustomTools() const {
            auto& registry = ToolRegistry::instance();
            auto& db = Database::instance();

            // 获取数据库中所有自定义工具名称（包括禁用的）
            auto allTools = db.getCustomTools();
            std::vector<std::string> allToolNames;
            for (const auto& t : allTools) {
                allToolNames.push_back(t.name);
            }

            // 先清除旧的自定义工具
            registry.clearCustomTools(allToolNames);

            // 只注册启用的工具
            auto tools = db.getEnabledCustomTools();
            int count = 0;

            for (const auto& tool : tools) {
                // 解析参数 JSON
                Json::Value params;
                if (!tool.parameters.empty()) {
                    Json::CharReaderBuilder builder;
                    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                    std::string errors;
                    reader->parse(tool.parameters.c_str(),
                                  tool.parameters.c_str() + tool.parameters.size(),
                                  &params, &errors);
                }

                // 根据执行类型注册不同的 handler
                if (tool.executorType == "python") {
                    registry.registerTool(
                        {
                            .name = tool.name,
                            .description = tool.description,
                            .parameters = params,
                            .handler = [script = tool.scriptContent](const Json::Value& args) -> drogon::Task<std::string> {
                                co_return co_await executePythonTool(script, args);
                            }
                        }, ToolCategory::INFORMATION
                    );
                } else if (tool.executorType == "http") {
                    registry.registerTool(
                        {
                            .name = tool.name,
                            .description = tool.description,
                            .parameters = params,
                            .handler = [config = tool.executorConfig](const Json::Value& args) -> drogon::Task<std::string> {
                                co_return co_await executeHttpTool(config, args);
                            }
                        }, ToolCategory::INFORMATION
                    );
                }

                count++;
                spdlog::info("ToolManager: 注册自定义工具 '{}' ({})", tool.name, tool.executorType);
            }

            spdlog::info("ToolManager: 自定义工具注册完成（共{}个）", count);
        }

        /// @brief 执行 Python 脚本工具
        /// @param scriptContent Python脚本内容（直接存储在数据库中）
        /// @param args 传入参数
        static drogon::Task<std::string> executePythonTool(const std::string& scriptContent, const Json::Value& args) {
            if (scriptContent.empty()) {
                co_return std::string("脚本内容为空");
            }

            // 获取配置的Python解释器路径
            std::string pythonPath = Database::instance().getCustomToolPython();

            // 构建输入参数 JSON
            Json::StreamWriterBuilder writerBuilder;
            writerBuilder["indentation"] = "";
            std::string inputJson = Json::writeString(writerBuilder, args);

            // 创建临时脚本文件
            std::string tmpScript = "/tmp/tool_" + std::to_string(std::rand()) + ".py";
            std::string tmpInput = "/tmp/tool_input_" + std::to_string(std::rand()) + ".json";

            // 写入脚本 - 去除开头可能的多余空白，保留内部缩进
            std::string cleanScript = scriptContent;
            // 去除开头的空白行
            size_t firstNonSpace = cleanScript.find_first_not_of(" \t\n\r");
            if (firstNonSpace != std::string::npos && firstNonSpace > 0) {
                cleanScript = cleanScript.substr(firstNonSpace);
            }

            {
                std::ofstream scriptFile(tmpScript);
                scriptFile << cleanScript;
            }
            {
                std::ofstream inputFile(tmpInput);
                inputFile << inputJson;
            }

            // 调试：打印脚本内容
            spdlog::debug("Python脚本内容:\n{}", cleanScript);

            // 执行: pythonPath script.py input.json
            std::string cmd = pythonPath + " " + tmpScript + " " + tmpInput + " 2>&1";
            spdlog::debug("执行Python工具: {}", cmd);

            std::array<char, 4096> buffer;
            std::string result;

            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                std::filesystem::remove(tmpScript);
                std::filesystem::remove(tmpInput);
                co_return std::string("执行脚本失败");
            }

            while (fgets(buffer.data(), buffer.size(), pipe)) {
                result += buffer.data();
            }
            int exitCode = pclose(pipe);

            // 清理临时文件
            std::filesystem::remove(tmpScript);
            std::filesystem::remove(tmpInput);

            if (exitCode != 0) {
                spdlog::warn("Python工具执行返回非零: {}, 输出: {}", exitCode, result);
            }

            // 移除末尾换行
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }

            co_return result;
        }

        /// @brief 执行 HTTP 工具
        static drogon::Task<std::string> executeHttpTool(const std::string& config, const Json::Value& args) {
            // 解析配置
            Json::Value configJson;
            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            std::string errors;

            if (!reader->parse(config.c_str(), config.c_str() + config.size(), &configJson, &errors)) {
                spdlog::error("HTTP工具配置解析失败: {}", errors);
                co_return std::string("工具配置错误");
            }

            std::string url = configJson.isMember("url") ? configJson["url"].asString() : "";
            std::string method = configJson.isMember("method") ? configJson["method"].asString() : "POST";
            int timeout = configJson.isMember("timeout") ? configJson["timeout"].asInt() : 30;

            if (url.empty()) {
                co_return std::string("未配置URL");
            }

            // 解析 URL
            // 格式: http://host:port/path 或 https://host:port/path
            size_t protoEnd = url.find("://");
            if (protoEnd == std::string::npos) {
                co_return std::string("URL格式错误");
            }
            std::string proto = url.substr(0, protoEnd);
            std::string rest = url.substr(protoEnd + 3);

            size_t pathStart = rest.find('/');
            std::string hostPort = pathStart == std::string::npos ? rest : rest.substr(0, pathStart);
            std::string path = pathStart == std::string::npos ? "/" : rest.substr(pathStart);

            // 创建 HTTP 客户端
            std::string baseUrl = proto + "://" + hostPort;
            auto client = drogon::HttpClient::newHttpClient(baseUrl);

            // 创建请求
            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(method == "GET" ? drogon::Get : drogon::Post);
            req->setPath(path);

            // 将 args 作为 JSON body 发送
            if (method != "GET" && !args.isNull()) {
                Json::StreamWriterBuilder writerBuilder;
                req->setBody(Json::writeString(writerBuilder, args));
                req->addHeader("Content-Type", "application/json");
            }

            try {
                auto resp = co_await client->sendRequestCoro(req);
                co_return std::string(resp->getBody());
            } catch (const std::exception& e) {
                spdlog::error("HTTP工具执行失败: {}", e.what());
                co_return std::string("HTTP请求失败: ") + e.what();
            }
        }

    private:
        AgentToolManager() = default;
    };
}

