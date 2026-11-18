#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <vector>

// one charge on its way through the sculk
struct SculkChargeCursor {
    Vector3i mPosition;
    int32_t mCharge = 0;
    int32_t mUpdateDelay = 0;
    int32_t mDecayDelay = 1;
    int32_t mFacing = -1;

    Tag save() const;

    // a missing field reads 0, not the default of a new cursor
    static SculkChargeCursor load(const Tag &data);
};

class SculkSpreader {
public:
    static constexpr size_t MAX_CURSORS = 32;
    static constexpr int32_t MAX_CHARGE = 1000;

    // split in cursors of at most MAX_CHARGE, a part past MAX_CURSORS is lost
    void addCursors(const Vector3i &position, int32_t charge);

    const std::vector<SculkChargeCursor> &getCursors() const {
        return mCursors;
    }

    int32_t getTotalCharge() const;

    // nothing when there is no cursor
    void save(Tag &data) const;

    // only into an empty spreader
    void load(const Tag &data);

private:
    std::vector<SculkChargeCursor> mCursors;
};
