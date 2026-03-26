#include <iostream>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    std::cout<<"0000"<<std::endl;
    // 1. 创建彩色控制台 sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::set_level(spdlog::level::debug);
    // 2. 可选：自定义每个日志级别的颜色
    console_sink->set_color(spdlog::level::info, console_sink->cyan);
    console_sink->set_color(spdlog::level::warn, console_sink->yellow);
    console_sink->set_color(spdlog::level::err, console_sink->red);
    console_sink->set_color(spdlog::level::debug, console_sink->magenta);

    // 3. 使用 vector 包装 sink
    auto logger = std::make_shared<spdlog::logger>(
        "console",
        spdlog::sinks_init_list{console_sink}  // 必须是列表
    );
    logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");

    // 4. 设置为全局默认 logger，保证在整个程序生命周期内有效
    spdlog::set_default_logger(logger);

    // 5. 启动 quit 线程
    std::jthread quit([]() {
        std::string input;
        while (std::cin >> input) {
            if (input == "quit") {
                drogon::app().quit();
                return;
            }
        }
    });

    try {
        drogon::app().addListener("0.0.0.0", 7778);
        drogon::app().loadConfigFile("../config.json");
        spdlog::info("Server started on port 3001");
        drogon::app().run();
    } catch (const std::exception &e) {
        spdlog::critical("程序发生错误: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("发生了未知错误。");
        return 1;
    }

    return 0;
}
