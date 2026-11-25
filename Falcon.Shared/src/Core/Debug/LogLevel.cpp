#include "Core/Debug/LogLevel.h"

const char *toString(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose:
            return "TRACE";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        default:
            return "INFO";
    }
}
