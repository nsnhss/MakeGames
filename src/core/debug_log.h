#pragma once
// debug_log.h — 流式调试日志宏（EDITOR_LOG），路由到 mg::core::Log。
// 输出同时进入：控制台（若已附加）、logs/engine.log、Console 面板（经 Log::GetHistory）。
// 原位于 src/editor/，engine_runtime 的 data_manager 也使用，故移至 core/ 消除越层依赖。
#include <string>
#include <sstream>
#include "core/log.h"

// EDITOR_LOG 宏：流式调试输出，等价于 Log::Debug。
#define EDITOR_LOG(msg) do { \
    std::stringstream ss; \
    ss << msg; \
    mg::core::Log::Debug(ss.str()); \
} while(0)
