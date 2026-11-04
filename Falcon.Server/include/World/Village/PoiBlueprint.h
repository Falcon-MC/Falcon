#pragma once

#include <cstdint>
#include <string>

/** Point of interest kinds, in the order the vanilla village manager indexes its per-type POI maps. */
enum class PoiType : int32_t {
    Bed = 0,
    MeetingArea = 1,
    JobSite = 2
};

/** What a block becomes when a dweller's scan turns it into a point of interest. */
struct PoiBlueprint {
    std::string mName;
    std::string mInitEvent;
    std::string mEndEvent;
    std::string mSoundEvent;
    PoiType mType;
    float mRadius;
    uint64_t mCapacity;
    uint64_t mWeight;
    bool mUseAABB;
};

class PoiBlueprints {
public:
    /** The blueprint registered for a block identifier, or nullptr when the block is not a point of interest. */
    static const PoiBlueprint *find(const std::string &identifier);
};
