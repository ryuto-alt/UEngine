#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace UnoEngine {

namespace {
    bool consoleInitialized = false;

    void InitializeConsole() {
        if (consoleInitialized) return;
#ifdef _WIN32
        // Set console output code page to UTF-8
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        consoleInitialized = true;
    }

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

    // Convert UTF-8 string to wide string (UTF-16)
    std::wstring Utf8ToWide(const std::string& utf8) {
#ifdef _WIN32
        if (utf8.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring wide(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], size);
        return wide;
#else
        // Simple fallback for non-Windows
        return std::wstring(utf8.begin(), utf8.end());
#endif
    }

    void OutputToDebugger(const std::string& message) {
#ifdef _WIN32
        // Use OutputDebugStringW for proper UTF-8/Unicode support
        std::wstring wide = Utf8ToWide(message + "\n");
        OutputDebugStringW(wide.c_str());
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

        InitializeConsole();

        std::string formatted = std::format("[{}] [{}] {}", 
            GetTimestamp(), LevelToString(level), message);

        // Console output (UTF-8)
        if (level >= LogLevel::Warning) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }

        // Visual Studio output window (Wide string)
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
