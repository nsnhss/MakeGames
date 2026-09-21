#pragma once
#include <string>
#include <sstream>
#include "core/log.h"

namespace mg {
namespace editor {

// 编辑器调试日志：路由到 mg::core::Log。
// 输出同时进入：控制台（若已附加）、logs/engine.log、Console 面板（经 Log::GetHistory）。
inline void EditorDebugLog(const std::string& msg) {
    mg::core::Log::Debug(msg);
}

// EDITOR_LOG 宏：流式调试输出，等价于 Log::Debug。
#define EDITOR_LOG(msg) do { \
    std::stringstream ss; \
    ss << msg; \
    mg::core::Log::Debug(ss.str()); \
} while(0)

} // namespace editor
} // namespace mg
