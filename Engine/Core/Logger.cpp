#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace UnoEngine {

namespace {
    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%H:%M:%S") << '.' 
            << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    void OutputToDebugger(const std::string& message) {
#ifdef _WIN32
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
#endif
    }

    const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }

    void Log(LogLevel level, const std::string& message, LogLevel currentLevel) {
        if (level < currentLevel) return;

        std::string formatted = std::format("[{}] [{}] {}", 
            GetTimestamp(), LevelToString(level), message);

        // Console output
        if (level >= LogLevel::Warning) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }

        // Visual Studio output window
        OutputToDebugger(formatted);
    }
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::Debug, message, currentLevel_);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::Info, message, currentLevel_);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::Warning, message, currentLevel_);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::Error, message, currentLevel_);
}

} // namespace UnoEngine
