/// @file Log.hpp
/// @brief 日志工具 - 形式日志输出
#pragma once
#include <chrono>
#include <source_location>
#include <print>
#include <iostream>
#include <format>
#include <string_view>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef ERROR
#undef ERROR
#endif

namespace LittleMeowBot {
    inline void initLogger(){
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_color(spdlog::level::info, consoleSink->cyan);
        consoleSink->set_color(spdlog::level::warn, consoleSink->yellow);
        consoleSink->set_color(spdlog::level::err, consoleSink->red);
        consoleSink->set_color(spdlog::level::debug, consoleSink->magenta);

        const auto logger = std::make_shared<spdlog::logger>(
            "console", spdlog::sinks_init_list{consoleSink});
        logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
    }

    inline std::string currentDateTime(){
        using namespace std::chrono;

        const auto now = system_clock::now();
        const zoned_time china_time{"Asia/Shanghai", now};
        const auto sec = floor<seconds>(china_time.get_local_time());
        const auto ms = duration_cast<milliseconds>(china_time.get_local_time() - sec).count();

        return std::format("{:%Y-%m-%d %H:%M:%S}.{:03}", sec, ms);
    }

    enum class LogLevel{
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    namespace fmt_color {
        using namespace std::string_view_literals;
        constexpr auto black = "\033[30m"sv;
        constexpr auto red = "\033[31m"sv;
        constexpr auto green = "\033[32m"sv;
        constexpr auto yellow = "\033[33m"sv;
        constexpr auto blue = "\033[34m"sv;
        constexpr auto magenta = "\033[35m"sv;
        constexpr auto cyan = "\033[36m"sv;
        constexpr auto white = "\033[37m"sv;
        constexpr auto gray = "\033[90m"sv;
        constexpr auto bright_red = "\033[91m"sv;
        constexpr auto bright_green = "\033[92m"sv;
        constexpr auto bright_yellow = "\033[93m"sv;
        constexpr auto bright_blue = "\033[94m"sv;
        constexpr auto bright_magenta = "\033[95m"sv;
        constexpr auto bright_cyan = "\033[96m"sv;
        constexpr auto bright_white = "\033[97m"sv;
        constexpr auto bg_red = "\033[41m"sv;
        constexpr auto bg_green = "\033[42m"sv;
        constexpr auto bg_yellow = "\033[43m"sv;
        constexpr auto bg_blue = "\033[44m"sv;
        constexpr auto bold = "\033[1m"sv;
        constexpr auto dim = "\033[2m"sv;
        constexpr auto italic = "\033[3m"sv;
        constexpr auto underline = "\033[4m"sv;
        constexpr auto reset = "\033[0m"sv;
    }

    class Log{
    public:
        explicit Log(LogLevel logLevel = LogLevel::INFO,
                     const std::source_location& loc = std::source_location::current())
            : level(logLevel), loc(loc){}

        template <typename... Args>
        void operator()(Args&&... args) const{
            if (m_styleOutPut) {
                std::print("{}[{:5}]{}", Color(level), Name(level), fmt_color::reset);
                std::print(" {}{}{} ", fmt_color::gray, currentDateTime(), fmt_color::reset);
                if (level != LogLevel::INFO) {
                    std::println("");
                    std::println("{}{}{}", fmt_color::dim,
                                 std::format(" - at {}:{} ({})", loc.file_name(), loc.line(), loc.function_name()),
                                 fmt_color::reset);
                    std::print(" {}- MSG:{} ", fmt_color::dim, fmt_color::reset);
                } else {
                    std::print(" ");
                }
                (std::print("{}{}{}", Color(level), std::forward<Args>(args), fmt_color::reset), ...);
            } else {
                std::print("[{:5}]", Name(level));
                std::print(" {} ", currentDateTime());
                if (level != LogLevel::INFO) {
                    std::println("");
                    std::println(
                        "{}", std::format("  - at {}:{}\n ({})", loc.file_name(), loc.line(), loc.function_name()));
                    std::print("-");
                } else {
                    std::print(" ");
                }
                (std::print("{}", std::forward<Args>(args)), ...);
            }
            std::println();
        }

        template <typename... Args>
        void fmt(std::format_string<Args...> fmt, Args&&... args) const{
            const std::string msg = std::format(fmt, std::forward<Args>(args)...);
            if (m_styleOutPut) {
                std::print("{}[{:5}]{}", Color(level), Name(level), fmt_color::reset);
                std::print(" {}{}{} ", fmt_color::gray, currentDateTime(), fmt_color::reset);
                if (level != LogLevel::INFO) {
                    std::println("");
                    std::println("{}{}{}", fmt_color::dim,
                                 std::format(" - at {}:{} ({})", loc.file_name(), loc.line(), loc.function_name()),
                                 fmt_color::reset);
                    std::print(" {}- MSG:{} ", fmt_color::dim, fmt_color::reset);
                } else {
                    std::print(" ");
                }
                std::print("{}{}{}", Color(level), msg, fmt_color::reset);
            } else {
                std::print("[{:5}]", Name(level));
                std::print(" {} ", currentDateTime());
                if (level != LogLevel::INFO) {
                    std::println("");
                    std::println(
                        "{}", std::format("  - at {}:{}\n ({})", loc.file_name(), loc.line(), loc.function_name()));
                    std::print("-");
                } else {
                    std::print(" ");
                }
                std::print("{}", msg);
            }
            std::println();
        }

        static void openStyleOutPut();
        static void closeStyleOutPut();

    private:
        static inline bool m_styleOutPut{false};
        LogLevel level = LogLevel::INFO;
        std::source_location loc;

        static constexpr std::string_view Color(LogLevel level){
            using enum LogLevel;
            switch (level) {
            case TRACE: return fmt_color::dim;
            case DEBUG: return fmt_color::bright_cyan;
            case INFO: return fmt_color::bright_green;
            case WARN: return fmt_color::bright_yellow;
            case ERROR: return fmt_color::bright_red;
            case FATAL: return "\033[1;41;97m";
            default: return fmt_color::reset;
            }
        }

        static constexpr std::string_view Name(LogLevel level){
            using enum LogLevel;
            switch (level) {
            case TRACE: return "TRACE";
            case DEBUG: return "DEBUG";
            case INFO: return "INFO";
            case WARN: return "WARN";
            case ERROR: return "ERROR";
            case FATAL: return "FATAL";
            default: return "UNKNOWN";
            }
        }
    };
}