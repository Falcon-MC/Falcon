#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <functional>
#include <vector>

class BlockState;

struct EndSpike {
    int32_t mX;
    int32_t mZ;
    int32_t mRadius;
    int32_t mHeight;
    bool mGuarded;

    // the box the fight searches for the crystal of this spike (sub_147169970)
    AxisAlignedBB getCrystalArea() const;

    Vector3i getTop() const;

    Vector3f getCrystalPosition() const;
};

/**
 * The ten obsidian spikes around the End island. The world generator, the dragon fight and the dragon all read the
 * same list, rebuilt from the level seed each time (sub_147169970).
 */
class EndSpikes {
public:
    using BlockSink = std::function<void(int32_t x, int32_t y, int32_t z, const BlockState &state)>;

    static constexpr int32_t COUNT = 10;

    static constexpr int32_t CLEARED_FROM_Y = 66;

    static std::vector<EndSpike> create(uint32_t seed);

    /**
     * The blocks of one spike without its crystal (sub_1471E8880). clearAbove also empties the column above y 66,
     * which the world generator and the respawn sequence do but the first dragon spawn does not.
     */
    static void place(const EndSpike &spike, bool clearAbove, const BlockSink &sink);

    static const BlockState &getCrystalBaseState();
};
