#include "log.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <filesystem>

namespace mg {
namespace core {

std::vector<Log::SinkFn> Log::sinks_;
std::vector<LogMessage> Log::history_;
bool Log::initialized_ = false;

static const char* LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

static std::string GetTimestamp() {
    auto now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&now));
    return buf;
}

void Log::Init() {
    if (initialized_) return;
    initialized_ = true;
    history_.reserve(1024);

    // 默认控制台输出
    AddSink([](const LogMessage& msg) {
        const char* color = "";
        const char* reset = "";
#ifdef _WIN32
        // Windows 控制台颜色通过 HANDLE 设置，这里简化处理
#else
        switch (msg.level) {
            case LogLevel::Error: color = "\033[31m"; break;
            case LogLevel::Warn:  color = "\033[33m"; break;
            case LogLevel::Info:  color = "\033[32m"; break;
            case LogLevel::Debug: color = "\033[36m"; break;
            case LogLevel::Trace: color = "\033[90m"; break;
        }
        reset = "\033[0m";
#endif
        std::cout << color << "[" << msg.timestamp << "] " << LevelToString(msg.level) << ": " << msg.text << reset << std::endl;
    });

    // 默认文件输出
    AddSink([](const LogMessage& msg) {
        static std::ofstream file;
        if (!file.is_open()) {
            std::filesystem::create_directories("logs");
            file.open("logs/engine.log", std::ios::app);
        }
        if (file.is_open()) {
            file << "[" << msg.timestamp << "] " << LevelToString(msg.level) << ": " << msg.text << "\n";
            file.flush();
        }
    });
}

void Log::Shutdown() {
    sinks_.clear();
    history_.clear();
    initialized_ = false;
}

void Log::AddSink(SinkFn sink) {
    sinks_.push_back(std::move(sink));
}

void Log::Write(LogLevel level, const std::string& msg) {
    if (!initialized_) Init();

    LogMessage entry;
    entry.level = level;
    entry.text = msg;
    entry.timestamp = GetTimestamp();

    history_.push_back(entry);
    if (history_.size() > 4096) {
        history_.erase(history_.begin(), history_.begin() + 1024);
    }

    for (auto& sink : sinks_) {
        sink(entry);
    }
}

void Log::Trace(const std::string& msg) { Write(LogLevel::Trace, msg); }
void Log::Debug(const std::string& msg) { Write(LogLevel::Debug, msg); }
void Log::Info(const std::string& msg)  { Write(LogLevel::Info, msg); }
void Log::Warn(const std::string& msg)  { Write(LogLevel::Warn, msg); }
void Log::Error(const std::string& msg) { Write(LogLevel::Error, msg); }

} // namespace core
} // namespace mg
