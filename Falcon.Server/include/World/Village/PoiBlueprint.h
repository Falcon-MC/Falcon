#pragma once

#include <cstdint>
#include <string>

// order of the game's per-type poi maps in the village manager
enum class PoiType : int32_t { Bed = 0, MeetingArea = 1, JobSite = 2 };

// what a dweller's scan turns the block into
struct PoiBlueprint {
    std::string mName;
    std::string mInitEvent;
    std::string mEndEvent;
    PoiType mType;
    uint64_t mCapacity;
    uint64_t mWeight;
};

class PoiBlueprints {
public:
    static const PoiBlueprint *find(const std::string &identifier);
};
