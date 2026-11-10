#include "World/Level/LevelGen/End/Feature/EndSpikes.h"

#include "World/Level/Block/VanillaBlocks.h"

#include <cmath>
#include <cstdlib>
#include <random>
#include <utility>

namespace {

    const double TWO_PI = 6.283185307179586;
    const float SPIKE_RING_RADIUS = 42.0f;
    const float CRYSTAL_AREA_TOP = 256.0f;

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &obsidianState() {
        static const BlockState state = VanillaBlocks::OBSIDIAN().toBlockState();
        return state;
    }

    const BlockState &ironBarsState() {
        static const BlockState state = VanillaBlocks::IRON_BARS().toBlockState();
        return state;
    }

    // the game reads its 65536 entry sine table
    float tableSine(float index) {
        return (float) std::sin((double) (uint16_t) (int32_t) index * TWO_PI / 65536.0);
    }

}

AxisAlignedBB EndSpike::getCrystalArea() const {
    return AxisAlignedBB((float) (mX - mRadius), 0.0f, (float) (mZ - mRadius), (float) (mX + mRadius), CRYSTAL_AREA_TOP,
                         (float) (mZ + mRadius));
}

Vector3i EndSpike::getTop() const {
    return Vector3i(mX, mHeight, mZ);
}

Vector3f EndSpike::getCrystalPosition() const {
    return Vector3f((float) mX + 0.5f, (float) mHeight + 1.0f, (float) mZ + 0.5f);
}

std::vector<EndSpike> EndSpikes::create(uint32_t seed) {
    std::mt19937 random(seed);

    int32_t sizes[COUNT];
    for (int32_t i = 0; i < COUNT; i++)
        sizes[i] = i;

    for (int32_t i = 1; i < COUNT; i++)
        std::swap(sizes[i], sizes[(int32_t) (random() % (uint32_t) (i + 1))]);

    std::vector<EndSpike> spikes;
    spikes.reserve(COUNT);

    for (int32_t i = 0; i < COUNT; i++) {
        const float angle = (float) i * 0.31415927f + -3.1415927f;
        const float index = (angle + angle) * 10430.378f;
        const int32_t size = sizes[i];
        spikes.push_back(EndSpike{(int32_t) (tableSine(index + 16384.0f) * SPIKE_RING_RADIUS),
                                  (int32_t) (tableSine(index) * SPIKE_RING_RADIUS), size / 3 + 2, size * 3 + 76,
                                  size == 1 || size == 2});
    }

    return spikes;
}

void EndSpikes::place(const EndSpike &spike, bool clearAbove, const BlockSink &sink) {
    const int32_t radius = spike.mRadius;

    for (int32_t x = -radius; x <= radius; x++) {
        for (int32_t y = 0; y <= spike.mHeight + 10; y++) {
            for (int32_t z = -radius; z <= radius; z++) {
                if (x * x + z * z <= radius * radius + 1 && y < spike.mHeight)
                    sink(spike.mX + x, y, spike.mZ + z, obsidianState());
                else if (clearAbove && y >= CLEARED_FROM_Y)
                    sink(spike.mX + x, y, spike.mZ + z, airState());
            }
        }
    }

    if (!spike.mGuarded)
        return;

    for (int32_t x = -2; x <= 2; x++) {
        for (int32_t z = -2; z <= 2; z++) {
            if (std::abs(x) == 2 || std::abs(z) == 2) {
                for (int32_t y = 0; y < 3; y++)
                    sink(spike.mX + x, spike.mHeight + y, spike.mZ + z, ironBarsState());
            }

            sink(spike.mX + x, spike.mHeight + 3, spike.mZ + z, ironBarsState());
        }
    }
}

const BlockState &EndSpikes::getCrystalBaseState() {
    // sub_1471E9580 sets the crystal on bedrock that keeps a fire burning
    static const BlockState state = []() {
        BlockState bedrock = VanillaBlocks::BEDROCK().toBlockState();
        bedrock.mStates.putByte("infiniburn_bit", 1);
        return BlockState(bedrock.mName, bedrock.mStates);
    }();
    return state;
}
