#include "World/Level/LevelGen/Overworld/Surface/NoiseGradientSurfaceBuilder.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &sulfurState() {
        static const BlockState state = VanillaBlocks::SULFUR().toBlockState();
        return state;
    }

    const BlockState &cinnabarState() {
        static const BlockState state = VanillaBlocks::CINNABAR().toBlockState();
        return state;
    }
}

// sub_1471326A0
NoiseGradientSurfaceBuilder::NoiseGradientSurfaceBuilder(int64_t levelSeed, const std::string &noiseName,
                                                         int32_t firstOctave, const std::vector<float> &amplitudes,
                                                         std::vector<Range> ranges,
                                                         std::vector<const BlockState *> nonReplaceable)
        : mRanges(std::move(ranges)), mNonReplaceable(std::move(nonReplaceable)) {
    XoroshiroRandom random(levelSeed);
    mNoise = NormalNoise::create(random.forkPositional(), noiseName, firstOctave, amplitudes);
}

// biomes/sulfur_caves.biome.json: surface and subsurface builders are identical
NoiseGradientSurfaceBuilder NoiseGradientSurfaceBuilder::sulfurCaves(int64_t levelSeed) {
    return NoiseGradientSurfaceBuilder(levelSeed, "minecraft:sulfur_cave_gradient", -5, {1.0f, 0.0f, 1.0f},
                                       {{-0.4f, -0.1f, &cinnabarState()},
                                        {0.0f, 0.4f, &sulfurState()},
                                        {0.4f, 1.0f, &cinnabarState()}},
                                       {&bedrockState()});
}

// sub_147132F40: clamped to [-1, 1], low bound excluded
const BlockState *NoiseGradientSurfaceBuilder::blockAt(int32_t x, int32_t y, int32_t z) const {
    const float noise = mNoise->getValue(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
    const float value = 1.0f < noise ? 1.0f : std::fmax(noise, -1.0f);
    for (const Range &range: mRanges) {
        if (value > range.mMin && value <= range.mMax)
            return range.mBlock;
    }

    return nullptr;
}

void NoiseGradientSurfaceBuilder::build(LevelChunk &chunk, int32_t x, int32_t z, int32_t fromY, int32_t toY) const {
    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    const int32_t top = std::min(toY - 1, chunk.getHeight(x, z));
    for (int32_t y = top; y >= fromY; y--) {
        const BlockState &state = chunk.getBlock(x, y, z);
        // BlockLegacy flag +41 & 2: liquids, the aquifer's water stays (seed 42 parity)
        if (state.getHash() == airState().getHash() || DecorationSupport::isLiquid(state))
            continue;

        const bool kept = std::any_of(mNonReplaceable.begin(), mNonReplaceable.end(), [&](const BlockState *block) {
            return block->mName == state.mName;
        });
        if (kept)
            continue;

        const BlockState *block = blockAt(worldX, y, worldZ);
        if (block != nullptr)
            chunk.setBlock(x, y, z, *block);
    }
}
