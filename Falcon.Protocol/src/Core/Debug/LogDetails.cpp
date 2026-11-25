#include "Core/Debug/LogDetails.h"

#include <cstdio>
#include <ctime>

std::string LogDetails::getTimestamp() const {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(mTime);

    const auto sinceEpoch = mTime.time_since_epoch();
    const long long milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch).count() % 1000;

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &seconds);
#else
    localtime_r(&seconds, &localTime);
#endif

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d:%03d",
             localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
             localTime.tm_hour, localTime.tm_min, localTime.tm_sec, (int) milliseconds);

    return std::string(buffer);
}
