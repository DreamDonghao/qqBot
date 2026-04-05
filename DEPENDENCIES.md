# LittleMeowBot 项目依赖

## 编译环境

- **CMake** >= 3.5
- **C++ 编译器** GCC 11+ (支持 C++20)
- **操作系统** Linux (Ubuntu 22.04 测试通过)

---

## 核心依赖

### 1. Drogon (Web 框架)

```bash
sudo apt install libdrogon-dev
```

- 版本: 1.8.0+
- 用途: HTTP 服务器、异步框架、协程支持

### 2. spdlog (日志库)

```bash
sudo apt install libspdlog-dev
```

- 版本: 1.9+
- 用途: 日志输出
- 链接方式: header-only

### 3. fmt (格式化库)

```bash
sudo apt install libfmt-dev
```

- 版本: 8.0+
- 用途: 字符串格式化

### 4. SQLite3 (数据库)

```bash
sudo apt install libsqlite3-dev
```

- 版本: 3.37+
- 用途: 本地数据存储

### 5. jsoncpp (JSON 解析)

```bash
sudo apt install libjsoncpp-dev
```

- 版本: 1.9+
- 用途: JSON 解析与生成

---

## Drogon 间接依赖

```bash
sudo apt install \
    libssl-dev \
    libzstd-dev \
    libbrotli-dev \
    libuuid1 \
    zlib1g-dev
```

| 库       | 用途       |
|---------|----------|
| OpenSSL | HTTPS 支持 |
| Brotli  | HTTP 压缩  |
| UUID    | 唯一标识符    |
| Zlib    | 压缩支持     |

---

## 可选依赖

### 静态链接支持

```bash
sudo apt install libstdc++-11-dev
```

---

## 一键安装 (Ubuntu 22.04)

```bash
sudo apt update && sudo apt install -y \
    cmake \
    g++ \
    libdrogon-dev \
    libspdlog-dev \
    libfmt-dev \
    libsqlite3-dev \
    libjsoncpp-dev \
    libssl-dev \
    libbrotli-dev \
    zlib1g-dev \
    libuuid1
```

---

## 编译命令

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

---

## 运行时依赖

可执行文件依赖以下动态库：

- `libfmt.so.8`
- `libsqlite3.so.0`
- `libjsoncpp.so.25`
- `libssl.so.3` / `libcrypto.so.3`
- `libbrotli*.so`
- `libc.so.6` (系统库)
- `libstdc++.so.6` (可静态链接)