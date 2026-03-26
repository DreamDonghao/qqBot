//
// Created by donghao on 2026/3/18.
//
#ifndef QQ_BOT_CONFIG_HPP
#define QQ_BOT_CONFIG_HPP

#include <json/config.h>
#include <json/value.h>
#include <string>
#include <spdlog/spdlog.h>

namespace qqBot {
    /// @brief 配置管理类，管理 API 密钥、模型配置等
    class Config{
    public:

        std::string plan_api_key = "";
        std::string plan_api_base_url = "";
        std::string plan_api_model = "";

        // DeepSeek API 配置
        std::string ds_api_key = "";
        std::string ds_api_base_url = "";
        std::string ds_api_model = "";

        // Qwen API 配置
        std::string qwen_api_key = "";
        std::string qwen_api_base_url = "";
        std::string qwen_api_model = "";

        // QQ Bot 配置
        std::string access_token = "";
        Json::UInt64 self_qq_number = 0;
        std::string qq_http_host = "";

        // 模型参数
        float model_temperature = 1.35f;
        float model_top_p = 0.92f;
        int model_max_tokens = 1024;

        // 记忆参数
        int memory_trigger_count = 10;
        int memory_chat_record_limit = 12;

        static Config& instance(){
            static Config config;
            return config;
        }

        void loadFromFile(const std::string& filepath) const{
            // TODO: 从文件加载配置
            spdlog::info("配置加载完成");
        }

    private:
        Config() = default;
    };
} // namespace qqBot

#endif //QQ_BOT_CONFIG_HPP
