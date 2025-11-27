#pragma once

#include <string>
#include <format>

namespace UnoEngine {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void SetLevel(LogLevel level) { currentLevel_ = level; }
    static LogLevel GetLevel() { return currentLevel_; }

    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);

    // Format support (C++20)
    template<typename... Args>
    static void Debug(std::format_string<Args...> fmt, Args&&... args) {
        Debug(std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args) {
        Info(std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warning(std::format_string<Args...> fmt, Args&&... args) {
        Warning(std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args) {
        Error(std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static inline LogLevel currentLevel_ = LogLevel::Info;
};

} // namespace UnoEngine
