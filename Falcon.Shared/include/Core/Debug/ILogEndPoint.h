#pragma once

#include "Core/Debug/LogDetails.h"

class ILogEndPoint {
public:
    virtual ~ILogEndPoint() = default;

    virtual void log(const LogDetails &details) = 0;

    virtual void flush() {}
};
