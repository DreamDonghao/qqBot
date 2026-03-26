# QQ Bot

基于 Drogon 框架的 QQ 机器人，支持多 LLM API 对话和记忆功能。

## 功能特性

- 支持多个 LLM API（DeepSeek、Qwen 等）
- 聊天记录管理和长期记忆
- 图片描述功能
- 群消息回复

## 环境要求

- C++20
- CMake
- Drogon
- spdlog
- jsoncpp

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 配置说明

### 1. LLM API 配置

编辑 `include/Config.hpp`，配置以下参数：

#### DeepSeek API
```cpp
std::string ds_api_key = "your-deepseek-api-key";
std::string ds_api_base_url = "https://api.deepseek.com";
std::string ds_api_model = "deepseek-chat";
```

#### Qwen API
```cpp
std::string qwen_api_key = "your-qwen-api-key";
std::string qwen_api_base_url = "https://dashscope.aliyuncs.com/compatible-mode/v1";
std::string qwen_api_model = "qwen-plus";
```

#### Plan API（备用模型）
```cpp
std::string plan_api_key = "your-plan-api-key";
std::string plan_api_base_url = "https://api.example.com/v1";
std::string plan_api_model = "model-name";
```

### 2. NapCat 配置

```cpp
std::string access_token = "your-access-token";  // NapCat access_token
Json::UInt64 self_qq_number = 123456789;          // 机器人 QQ 号
std::string qq_http_host = "http://127.0.0.1:3000"; // NapCat HTTP 服务地址
```

### 3. 图片描述功能配置

编辑 `include/QQMessage.hpp`：

```cpp
inline const std::string chat_api_key = "your-vision-api-key";
inline const std::string chat_api_base_url = "https://api.example.com/v1";
inline const std::string chat_api_model = "gpt-4-vision-preview";
```

### 4. 模型参数（可选）

```cpp
float model_temperature = 1.35f;  // 温度参数
float model_top_p = 0.92f;        // 核采样
int model_max_tokens = 1024;       // 最大 token 数
```

### 5. 记忆参数（可选）

```cpp
int memory_trigger_count = 10;     // 触发记忆保存的消息数
int memory_chat_record_limit = 12; // 聊天记录保留条数
```

## 运行

```bash
./qq_bot
```

服务默认监听 `0.0.0.0:7778`。

输入 `quit` 退出程序。

## 注意事项

- 请勿将 API Key 等敏感信息提交到 Git
- 建议使用环境变量或单独的配置文件管理密钥
- 确保 NapCat 已正确配置并运行

## 依赖项目

- [NapCat](https://github.com/NapNeko/NapCatQQ) - QQ 机器人框架
- [Drogon](https://github.com/drogonframework/drogon) - C++ Web 框架
- [spdlog](https://github.com/gabime/spdlog) - 日志库
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp) - JSON 解析库