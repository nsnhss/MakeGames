#pragma once
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace mg {
namespace core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

struct LogMessage {
    LogLevel level;
    std::string text;
    std::string timestamp;
};

// 简单日志系统：支持多 sink（控制台 + 文件），自动轮转历史。
class Log {
public:
    using SinkFn = std::function<void(const LogMessage&)>;

    static void Init();
    static void Shutdown();
    static void AddSink(SinkFn sink);
    static void Write(LogLevel level, const std::string& msg);

    static void Trace(const std::string& msg);
    static void Debug(const std::string& msg);
    static void Info(const std::string& msg);
    static void Warn(const std::string& msg);
    static void Error(const std::string& msg);

    // 历史消息（上限 4096 条，超限自动丢弃最旧的 1024 条），供 Console 面板等读取
    static const std::vector<LogMessage>& GetHistory() { return history_; }

private:
    static std::vector<SinkFn> sinks_;
    static std::vector<LogMessage> history_;
    static bool initialized_;
};

} // namespace core
} // namespace mg

// 安全日志宏：不依赖 stdout/stderr，避免 GUI 子进程中无效句柄导致二次崩溃
#define MG_LOG_INFO(msg) do { \
    std::stringstream ss; \
    ss << msg; \
    mg::core::Log::Info(ss.str()); \
} while(0)

#define MG_LOG_WARN(msg) do { \
    std::stringstream ss; \
    ss << msg; \
    mg::core::Log::Warn(ss.str()); \
} while(0)

#define MG_LOG_ERROR(msg) do { \
    std::stringstream ss; \
    ss << msg; \
    mg::core::Log::Error(ss.str()); \
} while(0)
