#pragma once

#include "Core/Debug/LogAreaID.h"
#include "Core/Debug/LogLevel.h"

#include <chrono>
#include <string>

struct LogDetails {
    LogAreaID mArea;
    LogLevel mLevel;
    std::string mMessage;
    const char *mFile;
    int mLine;
    std::chrono::system_clock::time_point mTime;

    LogDetails()
            : mArea(LogAreaID::All), mLevel(LogLevel::Info), mFile(nullptr), mLine(0),
              mTime(std::chrono::system_clock::now()) {}

    std::string getTimestamp() const;
};
