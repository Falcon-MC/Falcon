#include "World/Level/LevelGen/Overworld/Surface/MesaSurfaceBuilder.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr int32_t SEA_LEVEL = 63;
    // the builder's volume ends here, whatever the chunk's height
    constexpr int32_t VOLUME_TOP = LevelChunk::MIN_Y + 319;
    // stone blocks laid per column, counted from the first one
    constexpr int32_t MAX_STONE = 15;

    constexpr uint8_t WHITE = 0;
    constexpr uint8_t ORANGE = 1;
    constexpr uint8_t YELLOW = 4;
    constexpr uint8_t LIGHT_GRAY = 8;
    constexpr uint8_t BROWN = 12;
    constexpr uint8_t RED = 14;
    constexpr uint8_t HARD_CLAY = 16;

    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &coarseDirtState() {
        static const BlockState state = VanillaBlocks::COARSE_DIRT().toBlockState();
        return state;
    }

    const BlockState &grassState() {
        static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &hardenedClayState() {
        static const BlockState state = VanillaBlocks::HARDENED_CLAY().toBlockState();
        return state;
    }

    const BlockState &whiteTerracottaState() {
        static const BlockState state = VanillaBlocks::WHITE_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &orangeTerracottaState() {
        static const BlockState state = VanillaBlocks::ORANGE_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &yellowTerracottaState() {
        static const BlockState state = VanillaBlocks::YELLOW_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &lightGrayTerracottaState() {
        static const BlockState state = VanillaBlocks::LIGHT_GRAY_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &brownTerracottaState() {
        static const BlockState state = VanillaBlocks::BROWN_TERRACOTTA().toBlockState();
        return state;
    }

    const BlockState &redTerracottaState() {
        static const BlockState state = VanillaBlocks::RED_TERRACOTTA().toBlockState();
        return state;
    }

    // the bands only hold the colors drawn below
    const BlockState &clayOf(uint8_t color) {
        switch (color) {
            case WHITE:
                return whiteTerracottaState();
            case ORANGE:
                return orangeTerracottaState();
            case YELLOW:
                return yellowTerracottaState();
            case LIGHT_GRAY:
                return lightGrayTerracottaState();
            case BROWN:
                return brownTerracottaState();
            case RED:
                return redTerracottaState();
            default:
                return hardenedClayState();
        }
    }

    bool isState(const BlockState &state, const BlockState &other) {
        return state.getHash() == other.getHash();
    }

    // up to width blocks of color from a random band, cut at the last band
    void paintBands(std::array<uint8_t, 192> &bands, std::mt19937 &random, int32_t extraWidth, uint8_t color) {
        const int32_t count = GameRandom::nextInt(random, 10) + 5;
        for (int32_t i = 0; i <= count; i++) {
            const int32_t width = GameRandom::nextInt(random, 3) + extraWidth;
            const int32_t start = GameRandom::nextInt(random, 192);
            const int32_t length = std::min(width, 191 - start) + 1;
            std::fill_n(bands.begin() + start, length, color);
        }
    }

    // the game seeds the builder's noises from the level seed cut to 32 bits
    XoroshiroPositionalFactory noiseFactory(int64_t levelSeed) {
        XoroshiroRandom random(static_cast<int64_t>(static_cast<int32_t>(levelSeed)));
        return random.forkPositional();
    }
}

MesaSurfaceBuilder::MesaSurfaceBuilder(int64_t levelSeed) {
    const XoroshiroPositionalFactory factory = noiseFactory(levelSeed);
    mClayBandsOffsetNoise = NormalNoise::create(factory, "minecraft:clay_bands_offset", -8, {1.0f});
    mSurfaceNoise = NormalNoise::create(factory, "minecraft:badlands_surface", -6, {1.0f, 1.0f, 1.0f});
    mPillarNoise = NormalNoise::create(factory, "minecraft:badlands_pillar", -2, {1.0f, 1.0f, 1.0f, 1.0f});
    mPillarRoofNoise = NormalNoise::create(factory, "minecraft:badlands_pillar_roof", -8, {1.0f});

    std::mt19937 random(static_cast<uint32_t>(levelSeed));
    mBands.fill(HARD_CLAY);
    for (int32_t i = 0;;) {
        const int32_t band = i + GameRandom::nextInt(random, 5) + 1;
        if (band >= BAND_COUNT)
            break;
        mBands[(size_t) band] = ORANGE;
        if (band == BAND_COUNT - 1)
            break;
        i = band + 1;
    }

    paintBands(mBands, random, 0, YELLOW);
    paintBands(mBands, random, 1, BROWN);
    paintBands(mBands, random, 0, RED);

    // white bands: the first at 0, a light gray one on each side by chance, a step drawn even after the last
    const int32_t whiteCount = GameRandom::nextInt(random, 7) + 8;
    for (int32_t band = 0, placed = 0; band < BAND_COUNT; placed++) {
        mBands[(size_t) band] = WHITE;
        if (band >= 2 && (GameRandom::nextUnsigned(random) & 0x8000000u) != 0)
            mBands[(size_t) band - 1] = LIGHT_GRAY;
        if (band <= BAND_COUNT - 2 && (GameRandom::nextUnsigned(random) & 0x8000000u) != 0)
            mBands[(size_t) band + 1] = LIGHT_GRAY;

        const int32_t step = GameRandom::nextInt(random, 16);
        if (placed >= whiteCount)
            break;
        band += step + 4;
    }
}

uint8_t MesaSurfaceBuilder::_band(int32_t worldX, int32_t y, int32_t worldZ) const {
    const float offset = mClayBandsOffsetNoise->getValue((float) worldX, 0.0, (float) worldZ);
    // the index counts from the volume's bottom, not from y 0
    const int32_t index = y - LevelChunk::MIN_Y + BAND_COUNT + (int32_t) std::round(offset * 4.0f);
    return mBands[(size_t) (index % BAND_COUNT)];
}

float MesaSurfaceBuilder::_pillarTop(int32_t worldX, int32_t worldZ) const {
    const auto x = (float) worldX;
    const auto z = (float) worldZ;
    const float pillar = mPillarNoise->getValue(x * 0.2f, 0.0, z * 0.2f) * 15.0f;
    const float surface = std::fabs(mSurfaceNoise->getValue(x, 0.0, z) * 8.25f);
    const float body = std::min(surface, pillar);
    if (body <= 0.0f)
        return 0.0f;

    const float roof = std::fabs(mPillarRoofNoise->getValue(x * 0.75f, 0.0, z * 0.75f) * 1.5f) * 50.0f;
    return std::min(body * body * 2.5f, std::ceil(roof) + 24.0f) + 64.0f;
}

void MesaSurfaceBuilder::build(LevelChunk &chunk, int32_t x, int32_t z, int32_t lowestY,
                               const SurfaceBuilderComponent &component, float noise, std::mt19937 &random) const {
    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    const float pillarTop = component.mBrycePillars ? _pillarTop(worldX, worldZ) : 0.0f;

    const float depthNoise = noise / 3.0f;
    const auto depth = (int32_t) (GameRandom::nextFloat(random) * 0.25f + (depthNoise + 3.0f));
    const bool hardClay = std::cos(3.1415927f * depthNoise) > 0.0f;

    // a column topped by a liquid is left as it is
    int32_t top = VOLUME_TOP;
    while (top >= lowestY && DecorationSupport::isAir(chunk.getBlock(x, top, z)))
        top--;
    if (top >= lowestY && DecorationSupport::isLiquid(chunk.getBlock(x, top, z)))
        return;

    const SurfaceMaterialAttributes &material = component.mSurfaceMaterial;
    const auto fillTop = (int32_t) std::floor(pillarTop);
    const BlockState *layer = material.mMidBlock;
    int32_t remaining = -1;
    int32_t stone = 0;
    bool sandAbove = false;
    bool solidAbove = false;
    for (int32_t y = VOLUME_TOP; y >= lowestY; y--) {
        BlockState state = chunk.getBlock(x, y, z);
        if (isState(state, bedrockState()))
            return;

        // the pillar fills the air down to the ground; those blocks don't count as stone met
        bool counted = true;
        if (DecorationSupport::isSolid(state)) {
            solidAbove = true;
        } else if (!solidAbove && y <= fillTop) {
            state = stoneState();
            chunk.setBlock(x, y, z, state);
            counted = false;
        }

        if (stone >= MAX_STONE)
            continue;

        if (DecorationSupport::isAir(state)) {
            remaining = -1;
            continue;
        }
        if (!isState(state, stoneState()))
            continue;

        if (counted)
            stone++;

        if (remaining != -1) {
            if (remaining <= 0)
                continue;

            remaining--;
            if (sandAbove) {
                chunk.setBlock(x, y, z, orangeTerracottaState());
                continue;
            }
            chunk.setBlock(x, y, z, clayOf(_band(worldX, y, worldZ)));
            continue;
        }

        if (y >= SEA_LEVEL - 4 && y <= SEA_LEVEL + 1)
            layer = material.mMidBlock;
        if (depth <= 0)
            layer = material.mFoundationBlock;
        remaining = depth + std::max(0, y - SEA_LEVEL);
        sandAbove = false;

        if (y < SEA_LEVEL - 1) {
            chunk.setBlock(x, y, z, isState(*layer, whiteTerracottaState()) ? orangeTerracottaState() : *layer);
            continue;
        }

        if (component.mHasForest && y > SEA_LEVEL + 2 * depth + 33) {
            chunk.setBlock(x, y, z, hardClay ? coarseDirtState() : grassState());
            continue;
        }

        if (y <= SEA_LEVEL + depth + 10) {
            chunk.setBlock(x, y, z, *material.mTopBlock);
            sandAbove = true;
            continue;
        }

        uint8_t color = ORANGE;
        if (y > SEA_LEVEL && y <= SEA_LEVEL + 192)
            color = hardClay ? HARD_CLAY : _band(worldX, y, worldZ);
        chunk.setBlock(x, y, z, clayOf(color));
    }
}
