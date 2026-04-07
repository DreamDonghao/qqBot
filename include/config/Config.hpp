/// @file Config.hpp
/// @brief 全局配置管理
/// @author donghao
/// @date 2026-04-02
/// @details 管理所有 LLM 模型配置、记忆系统参数、知识库配置等。
///          配置从 SQLite 数据库加载，支持运行时更新。

#pragma once

#include <json/value.h>
#include <storage/Database.hpp>
#include <string>
#include <spdlog/spdlog.h>

namespace LittleMeowBot {
    /// @brief LLM API 配置结构体
    /// @details 包含连接 LLM API 所需的基本信息
    struct LLMApiConfig {
        std::string apiKey;     ///< API 密钥
        std::string baseUrl;    ///< API 基础 URL
        std::string path;       ///< API 路径
        std::string model;      ///< 模型名称
    };

    /// @brief LLM 模型参数配置
    /// @details 控制模型生成行为的参数
    struct LLMModelParams {
        int maxTokens = 1024;       ///< 最大生成 token 数
        float temperature = 0.7f;   ///< 温度参数，控制随机性
        float topP = 0.9f;          ///< Top-P 采样参数
    };

    /// @brief 知识库 API 配置
    /// @details RAGFlow 知识库连接参数
    struct KBApiConfig {
        std::string apiKey;             ///< RAGFlow API 密钥
        std::string baseUrl;            ///< RAGFlow 服务地址
        std::string knowledgeDatasetId; ///< 知识库 ID（Agent 工具调用）
        std::string memoryDatasetId;    ///< 记忆库 ID（长期记忆存储）
        std::string memoryDocumentId;   ///< 记忆文档 ID（添加 chunk）
    };

    /// @brief 配置管理类（单例模式）
    /// @details 集中管理所有配置，支持从数据库加载和运行时更新
    class Config {
    public:
        // ============== 多层代理架构配置 ==============

        /// @name Router Agent 配置
        /// @{
        LLMApiConfig router;           ///< Router API 配置
        LLMModelParams routerParams;   ///< Router 模型参数
        /// @}

        /// @name Planner Agent 配置
        /// @{
        LLMApiConfig planner;           ///< Planner API 配置
        LLMModelParams plannerParams;   ///< Planner 模型参数
        /// @}

        /// @name Executor Agent 配置
        /// @{
        LLMApiConfig executor;           ///< Executor API 配置
        LLMModelParams executorParams;   ///< Executor 模型参数
        /// @}

        // ============== 记忆服务配置 ==============

        /// @name Memory Service 配置
        /// @{
        LLMApiConfig memory;            ///< Memory API 配置
        LLMModelParams memoryParams;    ///< Memory 模型参数
        int memoryTriggerCount = 16;    ///< 触发记忆生成的消息间隔
        int memoryChatRecordLimit = 18; ///< 保留的聊天记录数量
        int shortTermMemoryMax = 15;    ///< 短期记忆上限（触发迁移阈值）
        int shortTermMemoryLimit = 20;  ///< 合并时保留的最大条数
        int memoryMigrateCount = 5;     ///< 每次迁移到长期记忆的条数
        /// @}

        // ============== 图片处理配置 ==============

        /// @name Image Processing 配置
        /// @{
        LLMApiConfig image;  ///< 多模态模型配置（图片识别）
        /// @}

        // ============== QQ Bot 基础配置 ==============

        /// @name QQ Bot 基础配置
        /// @{
        std::string accessToken;                     ///< OneBot API 访问令牌
        std::uint64_t selfQQNumber = 0;              ///< Bot 的 QQ 号
        std::string qqHttpHost;                      ///< OneBot HTTP 服务地址
        std::string botName = "小喵";                 ///< Bot 名称
        /// @}

        // ============== 知识库配置 ==============

        KBApiConfig knowledgeBase;  ///< 知识库配置（从数据库加载）

        /// @brief 获取单例实例
        /// @return 配置实例引用
        static Config& instance() {
            static Config config{};
            return config;
        }

        /// @brief 从数据库加载单个 LLM 配置
        /// @tparam ApiCfg API 配置类型
        /// @tparam ParamsCfg 模型参数类型（可选）
        /// @param name 配置名称（router/planner/executor/memory/image）
        /// @param apiConfig API 配置对象引用
        /// @param modelParams 模型参数对象指针（可选）
        template<typename ApiCfg, typename ParamsCfg = LLMModelParams*>
        void loadLLMConfig(const std::string_view name, ApiCfg& apiConfig, ParamsCfg modelParams = nullptr) {
            auto cfg = Database::instance().getLLMConfig(std::string(name));
            if (cfg.isNull()) return;

            apiConfig.apiKey = cfg["apiKey"].asString();
            apiConfig.baseUrl = cfg["baseUrl"].asString();
            apiConfig.path = cfg["path"].asString();
            apiConfig.model = cfg["model"].asString();

            if constexpr (!std::is_null_pointer_v<ParamsCfg>) {
                if (modelParams) {
                    modelParams->maxTokens = cfg["maxTokens"].asInt();
                    modelParams->temperature = cfg["temperature"].asFloat();
                    modelParams->topP = cfg["topP"].asFloat();
                }
            }
        }

        /// @brief 从数据库加载所有配置
        /// @details 加载 LLM 配置、知识库配置、记忆配置
        void loadFromDatabase() {
            loadLLMConfig("router", router, &routerParams);
            loadLLMConfig("planner", planner, &plannerParams);
            loadLLMConfig("executor", executor, &executorParams);
            loadLLMConfig("memory", memory, &memoryParams);
            loadLLMConfig("image", image);

            // 加载知识库配置
            if (auto kbCfg = Database::instance().getKBConfig();
                !kbCfg.isNull()) {
                knowledgeBase.apiKey = kbCfg["apiKey"].asString();
                knowledgeBase.baseUrl = kbCfg["baseUrl"].asString();
                knowledgeBase.knowledgeDatasetId = kbCfg["knowledgeDatasetId"].asString();
                knowledgeBase.memoryDatasetId = kbCfg["memoryDatasetId"].asString();
                if (kbCfg.isMember("memoryDocumentId")) {
                    knowledgeBase.memoryDocumentId = kbCfg["memoryDocumentId"].asString();
                }
                spdlog::info("知识库配置已从数据库加载");
            }

            // 加载记忆配置
            if (auto memCfg = Database::instance().getMemoryConfig();
                !memCfg.isNull()) {
                memoryTriggerCount = memCfg["memoryTriggerCount"].asInt();
                memoryChatRecordLimit = memCfg["memoryChatRecordLimit"].asInt();
                shortTermMemoryMax = memCfg["shortTermMemoryMax"].asInt();
                shortTermMemoryLimit = memCfg["shortTermMemoryLimit"].asInt();
                memoryMigrateCount = memCfg["memoryMigrateCount"].asInt();
                spdlog::info("记忆配置已从数据库加载");
            }

            // 加载 QQ Bot 配置
            if (auto qqCfg = Database::instance().getQQConfig();
                !qqCfg.isNull()) {
                accessToken = qqCfg["accessToken"].asString();
                selfQQNumber = qqCfg["selfQQNumber"].asInt64();
                qqHttpHost = qqCfg["qqHttpHost"].asString();
                if (qqCfg.isMember("botName")) {
                    botName = qqCfg["botName"].asString();
                }
                spdlog::info("QQ Bot 配置已从数据库加载");
            }

            spdlog::info("所有配置已从数据库加载");
        }

    private:
        Config() = default;
    };
} // namespace LittleMeowBot