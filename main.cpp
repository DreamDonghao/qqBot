#include <iostream>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <util/Log.hpp>
#include <storage/Database.hpp>
#include <config/Config.hpp>
#include <agent/AgentSystem.hpp>
#include <model/QQMessage.hpp>

int main(){
    // 初始化日志
    LittleMeowBot::initLogger();

    try {
        // ========== 系统初始化 ==========
        auto& database = LittleMeowBot::Database::instance();
        database.initialize("../data/little_meow_bot.db");

        auto& config = LittleMeowBot::Config::instance();
        config.loadFromDatabase();

        // 初始化 QQ 昵称
        LittleMeowBot::QQMessage::setCustomQQName(config.selfQQNumber, config.botName + "(我)");

        // 初始化 Agent 系统
        LittleMeowBot::AgentSystem::instance().initialize();

        spdlog::info("系统初始化完成 - 启用群: {}, 管理员: {}",
            database.getEnabledGroups().size(),
            database.getAdmins().size());

        // ========== 启动服务 ==========
        // 启动 quit 线程
        std::jthread quit([]() {
            std::string input;
            while (std::cin >> input) {
                if (input == "quit") {
                    drogon::app().quit();
                    return;
                }
            }
        });

        drogon::app().addListener("0.0.0.0", 7778);
        drogon::app().setDocumentRoot("../public");
        spdlog::info("Server started on port 7778");
        spdlog::info("Admin page: http://localhost:7778/index.html");

        drogon::app().run();

        // ========== 清理 ==========
        database.close();
        spdlog::info("系统已关闭");

    } catch (const std::exception& e) {
        spdlog::critical("程序发生错误: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("发生了未知错误");
        return 1;
    }

    return 0;
}