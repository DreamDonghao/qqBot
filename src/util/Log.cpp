/// @file Log.cpp
/// @brief 日志工具 - 实现

#include <util/Log.hpp>

namespace LittleMeowBot {
    void Log::openStyleOutPut(){
        m_styleOutPut = true;
    }

    void Log::closeStyleOutPut(){
        m_styleOutPut = false;
    }
}