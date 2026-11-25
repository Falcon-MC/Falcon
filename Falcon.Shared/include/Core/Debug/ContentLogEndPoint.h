#pragma once

#include "Core/Debug/ILogEndPoint.h"

#include <cstddef>
#include <mutex>

class ContentLogEndPoint : public ILogEndPoint {
public:
    static constexpr size_t STDOUT_BUFFER_SIZE = 65536;
    static constexpr size_t LINE_PREFIX_RESERVE = 64;

    explicit ContentLogEndPoint(bool useColor = true);

    void log(const LogDetails &details) override;

    void flush() override;

private:
    static const char *_getColorCode(LogLevel level);

    bool mUseColor;
    std::mutex mMutex;
};
