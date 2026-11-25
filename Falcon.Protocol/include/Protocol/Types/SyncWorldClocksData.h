#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SyncWorldClockStateData {
public:
    uint64_t mClockId = 0;
    int32_t mTime = 0;
    bool mPaused = false;
};

class TimeMarkerData {
public:
    uint64_t mId = 0;
    std::string mName;
    int32_t mTime = 0;
    bool mHasPeriod = false;
    int32_t mPeriod = 0;
};

class WorldClockData {
public:
    uint64_t mId = 0;
    std::string mName;
    int32_t mTime = 0;
    bool mPaused = false;
    std::vector<TimeMarkerData> mTimeMarkers;
};
