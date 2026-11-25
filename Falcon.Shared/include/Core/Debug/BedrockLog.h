#pragma once

#include "Core/Debug/ILogEndPoint.h"
#include "Core/Debug/LogAreaID.h"
#include "Core/Debug/LogLevel.h"

#include <cstdarg>
#include <memory>

#if defined(__GNUC__) || defined(__clang__)
#define BEDROCK_LOG_PRINTF_FORMAT(formatIndex, firstArgIndex) \
    __attribute__((format(printf, formatIndex, firstArgIndex)))
#else
#define BEDROCK_LOG_PRINTF_FORMAT(formatIndex, firstArgIndex)
#endif

namespace BedrockLog {

    enum class LogCategory : int {
        All = 0,
        Automation = 1
    };

    enum class LogRule : int {
        Default = 0,
        IgnoreLevel = 1
    };

    void addEndPoint(const std::shared_ptr<ILogEndPoint> &endPoint);

    void removeEndPoint(const std::shared_ptr<ILogEndPoint> &endPoint);

    void clearEndPoints();

    void setLogLevel(LogLevel level);

    LogLevel getLogLevel();

    void log(LogCategory category, LogRule rule, LogAreaID area, LogLevel level, const char *file, int line,
             const char *format, ...) BEDROCK_LOG_PRINTF_FORMAT(7, 8);

    void log_va(LogCategory category, LogRule rule, LogAreaID area, LogLevel level, const char *file, int line,
                const char *format, va_list args);

    void flush();

    void shutdown();

}

#define LOG_TRACE(area, ...) \
    BedrockLog::log(BedrockLog::LogCategory::All, BedrockLog::LogRule::Default, area, LogLevel::Verbose, \
                    __FILE__, __LINE__, __VA_ARGS__)

#define LOG_INFO(area, ...) \
    BedrockLog::log(BedrockLog::LogCategory::All, BedrockLog::LogRule::Default, area, LogLevel::Info, \
                    __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARN(area, ...) \
    BedrockLog::log(BedrockLog::LogCategory::All, BedrockLog::LogRule::Default, area, LogLevel::Warning, \
                    __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(area, ...) \
    BedrockLog::log(BedrockLog::LogCategory::All, BedrockLog::LogRule::Default, area, LogLevel::Error, \
                    __FILE__, __LINE__, __VA_ARGS__)

#define LOG_FATAL(area, ...) \
    BedrockLog::log(BedrockLog::LogCategory::All, BedrockLog::LogRule::Default, area, LogLevel::Fatal, \
                    __FILE__, __LINE__, __VA_ARGS__)
