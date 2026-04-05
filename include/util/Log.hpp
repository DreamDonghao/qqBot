/// @file Log.hpp
/// @brief 日志工具 - 彩色日志输出
/// @author donghao
/// @date 2026-04-02
/// @details 提供彩色日志输出功能：
///          - 多级别日志：TRACE, DEBUG, INFO, WARN, ERROR, FATAL
///          - 彩色输出：支持前景色、背景色、样式
///          - 调用点追踪：自动记录文件名、行号、函数名

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
    /// @brief 初始化 spdlog 彩色日志
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

    /// @brief 日志级别
    enum class LogLevel{
        TRACE, ///< 最细粒度的调试信息，记录程序的每一步执行
        DEBUG, ///< 调试信息，帮助理解程序运行状态
        INFO, ///< 常规信息，记录程序正常运行的重要事件
        WARN, ///< 警告信息，表示可能出现问题但不影响程序继续运行
        ERROR, ///< 错误信息，表示程序遇到了问题，但可以继续运行
        FATAL ///< 致命错误，程序无法继续运行，即将退出
    };

    // fmt 颜色样式定义
    namespace fmt_color {
        using namespace std::string_view_literals;

        // 前景色
        constexpr auto black = "\033[30m"sv; ///< 黑色
        constexpr auto red = "\033[31m"sv; ///< 红色
        constexpr auto green = "\033[32m"sv; ///< 绿色
        constexpr auto yellow = "\033[33m"sv; ///< 黄色
        constexpr auto blue = "\033[34m"sv; ///< 蓝色
        constexpr auto magenta = "\033[35m"sv; ///< 品红色
        constexpr auto cyan = "\033[36m"sv; ///< 青色
        constexpr auto white = "\033[37m"sv; ///< 白色
        constexpr auto gray = "\033[90m"sv; ///< 灰色（亮黑色）

        // 亮色
        constexpr auto bright_red = "\033[91m"sv; ///< 亮红色
        constexpr auto bright_green = "\033[92m"sv; ///< 亮绿色
        constexpr auto bright_yellow = "\033[93m"sv; ///< 亮黄色
        constexpr auto bright_blue = "\033[94m"sv; ///< 亮蓝色
        constexpr auto bright_magenta = "\033[95m"sv; ///< 亮品红色
        constexpr auto bright_cyan = "\033[96m"sv; ///< 亮青色
        constexpr auto bright_white = "\033[97m"sv; ///< 亮白色

        // 背景色
        constexpr auto bg_red = "\033[41m"sv; ///< 红色背景
        constexpr auto bg_green = "\033[42m"sv; ///< 绿色背景
        constexpr auto bg_yellow = "\033[43m"sv; ///< 黄色背景
        constexpr auto bg_blue = "\033[44m"sv; ///< 蓝色背景

        // 样式
        constexpr auto bold = "\033[1m"sv; ///< 粗体
        constexpr auto dim = "\033[2m"sv; ///< 暗淡
        constexpr auto italic = "\033[3m"sv; ///< 斜体
        constexpr auto underline = "\033[4m"sv; ///< 下划线
        constexpr auto reset = "\033[0m"sv; ///< 重置所有样式
    }

    class Log{
    public:
        // 构造函数默认捕获调用点位置
        explicit Log(const LogLevel logLevel = LogLevel::INFO,
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
                                 std::format(" ├─ at {}:{} ({})", loc.file_name(), loc.line(), loc.function_name()),
                                 fmt_color::reset);
                    std::print(" {}└─ MSG:{} ", fmt_color::dim, fmt_color::reset);
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
                        "{}", std::format("  └─ at {}:{}\n ({})", loc.file_name(), loc.line(), loc.function_name()));
                    std::print("└─");
                } else {
                    std::print(" ");
                }
                (std::print("{}", std::forward<Args>(args)), ...);
            }
            std::println();
        }

        /// @brief fmt 风格格式化输出
        /// @tparam Args 参数类型
        /// @param fmt 格式字符串，使用 {} 占位符
        /// @param args 参数值
        template <typename... Args>
        void fmt(std::format_string<Args...> fmt, Args&&... args) const{
            const std::string msg = std::format(fmt, std::forward<Args>(args)...);
            if (m_styleOutPut) {
                std::print("{}[{:5}]{}", Color(level), Name(level), fmt_color::reset);
                std::print(" {}{}{} ", fmt_color::gray, currentDateTime(), fmt_color::reset);
                if (level != LogLevel::INFO) {
                    std::println("");
                    std::println("{}{}{}", fmt_color::dim,
                                 std::format(" ├─ at {}:{} ({})", loc.file_name(), loc.line(), loc.function_name()),
                                 fmt_color::reset);
                    std::print(" {}└─ MSG:{} ", fmt_color::dim, fmt_color::reset);
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
                        "{}", std::format("  └─ at {}:{}\n ({})", loc.file_name(), loc.line(), loc.function_name()));
                    std::print("└─");
                } else {
                    std::print(" ");
                }
                std::print("{}", msg);
            }
            std::println();
        }

        static void openStyleOutPut(){
            m_styleOutPut = true;
        }

        static void closeStyleOutPut(){
            m_styleOutPut = false;
        }

    private:
        static inline bool m_styleOutPut{false};
        LogLevel level = LogLevel::INFO;
        std::source_location loc;

        static constexpr std::string_view Color(const LogLevel level){
            using enum LogLevel;
            switch (level) {
            case TRACE: return fmt_color::dim; ///< 暗淡，低优先级
            case DEBUG: return fmt_color::bright_cyan; ///< 亮青色，调试信息
            case INFO: return fmt_color::bright_green; ///< 亮绿色，正常状态
            case WARN: return fmt_color::bright_yellow; ///< 亮黄色，警告
            case ERROR: return fmt_color::bright_red; ///< 亮红色，错误
            case FATAL: return "\033[1;41;97m"; ///< 粗体+红底+亮白字，致命错误
            default: return fmt_color::reset;
            }
        }

        static constexpr std::string_view Name(const LogLevel level){
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
} // namespace LittleMeowBot
