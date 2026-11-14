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
    // a mob is at the point within this distance, or touching the block's box grown by it
    float mRadius = 0.0f;
    bool mUseBoundingBox = false;
    // played by a villager working there
    std::string mSound;
};

class PoiBlueprints {
public:
    // the event a lost job site fires
    static constexpr const char *BECOME_UNSKILLED = "minecraft:become_unskilled";

    static const PoiBlueprint *find(const std::string &identifier);
};
