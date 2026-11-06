#include "World/Level/LevelGen/Nether/Surface/NetherSurfaceBuilder.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <algorithm>

namespace {
    const int32_t HEIGHT = 128;
    const int32_t SEA_LEVEL = NetherGeneratorConstants::LAVA_LEVEL + 1;

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &netherrackState() {
        static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    const BlockState &soulSandState() {
        static const BlockState state = VanillaBlocks::SOUL_SAND().toBlockState();
        return state;
    }

    const BlockState &soulSoilState() {
        static const BlockState state = VanillaBlocks::SOUL_SOIL().toBlockState();
        return state;
    }

    const BlockState &gravelState() {
        static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
        return state;
    }

    const BlockState &basaltState() {
        static const BlockState state = VanillaBlocks::BASALT().toBlockState();
        return state;
    }

    const BlockState &blackstoneState() {
        static const BlockState state = VanillaBlocks::BLACKSTONE().toBlockState();
        return state;
    }

    const BlockState &crimsonNyliumState() {
        static const BlockState state = VanillaBlocks::CRIMSON_NYLIUM().toBlockState();
        return state;
    }

    const BlockState &warpedNyliumState() {
        static const BlockState state = VanillaBlocks::WARPED_NYLIUM().toBlockState();
        return state;
    }

    const BlockState &netherWartState() {
        static const BlockState state = VanillaBlocks::NETHER_WART_BLOCK().toBlockState();
        return state;
    }

    bool sameBlock(const BlockState &first, const BlockState &second) {
        return first.getHash() == second.getHash();
    }

    bool isSolid(const BlockState &state) {
        return !sameBlock(state, airState()) && !sameBlock(state, lavaState());
    }

    std::vector<CappedNetherSurface::Material> makeMaterials(const std::vector<const BlockState *> &blocks,
                                                             uint32_t &seed) {
        std::vector<CappedNetherSurface::Material> materials;
        materials.reserve(blocks.size());
        for (const BlockState *block: blocks) {
            std::mt19937 random(seed++);
            materials.push_back({block, std::make_unique<TheEndOctaveNoise>(random, 1)});
        }

        return materials;
    }

    const BlockState *pickMaterial(const std::vector<CappedNetherSurface::Material> &materials, float x, float y,
                                   float z) {
        if (materials.size() == 1)
            return materials[0].mBlock;

        const BlockState *picked = materials[0].mBlock;
        float best = -1.0f;
        for (const CappedNetherSurface::Material &material: materials) {
            const float value = material.mNoise->sample(x, y, z);
            if (value > best)
                picked = material.mBlock;
            best = std::max(value, best);
        }

        return picked;
    }
}

CappedNetherSurface::CappedNetherSurface(uint32_t seed, const std::vector<const BlockState *> &floor,
                                         const std::vector<const BlockState *> &ceiling, const BlockState *beach,
                                         const TheEndOctaveNoise &beachNoise)
        : mBeach(beach), mBeachNoise(beachNoise) {
    mFloor = makeMaterials(floor, seed);
    mCeiling = makeMaterials(ceiling, seed);
}

void CappedNetherSurface::build(const NetherSurfaceColumn &column) const {
    LevelChunk &chunk = column.mChunk;
    const int32_t worldX = (chunk.getX() << 4) + column.mX;
    const int32_t worldZ = (chunk.getZ() << 4) + column.mZ;

    const float base = column.mDepthNoise / 3.0f + 3.0f;
    const float floorExtra = GameRandom::nextFloat(column.mRandom) * 0.25f;
    const float ceilingExtra = GameRandom::nextFloat(column.mRandom) * 0.25f;
    const int32_t floorDepth = (int32_t) (base + floorExtra);
    const int32_t ceilingDepth = (int32_t) (base + ceilingExtra);

    const float sampleX = (float) worldX * 0.0625f;
    const float sampleZ = (float) worldZ * 0.0625f;
    const BlockState *floor = pickMaterial(mFloor, sampleX, (float) SEA_LEVEL, sampleZ);
    const BlockState *ceiling = pickMaterial(mCeiling, sampleX, (float) SEA_LEVEL, sampleZ);

    const bool beach = mBeach != nullptr
                       && mBeachNoise.sample((float) worldX * 0.03125f, 109.0f, (float) worldZ * 0.03125f) > 0.0f;

    bool wasSolid = isSolid(chunk.getBlock(column.mX, 0, column.mZ));
    for (int32_t y = 0; y < HEIGHT; y++) {
        const bool solid = isSolid(chunk.getBlock(column.mX, y, column.mZ));
        if (solid == wasSolid)
            continue;

        wasSolid = solid;
        if (!solid) {
            const int32_t end = std::max(-1, y - 1 - floorDepth);
            for (int32_t fill = y - 1; fill != end; fill--) {
                if (!isSolid(chunk.getBlock(column.mX, fill, column.mZ)))
                    break;

                const bool nearSea = fill <= SEA_LEVEL && fill >= SEA_LEVEL - 5;
                chunk.setBlock(column.mX, fill, column.mZ, beach && nearSea ? *mBeach : *floor);
            }

            continue;
        }

        const int32_t end = std::min(HEIGHT, y + ceilingDepth);
        for (int32_t fill = y; fill < end; fill++) {
            if (!isSolid(chunk.getBlock(column.mX, fill, column.mZ)))
                break;

            chunk.setBlock(column.mX, fill, column.mZ, *ceiling);
        }
    }
}

LayeredNetherSurface::LayeredNetherSurface(const std::vector<SurfaceSimplexNoise> &adjustmentNoise,
                                           const BlockState &top, const BlockState &mid, const BlockState &seaFloor,
                                           int32_t seaFloorDepth, std::vector<Adjustment> adjustments)
        : mAdjustmentNoise(adjustmentNoise), mTop(&top), mMid(&mid), mSeaFloor(&seaFloor),
          mSeaFloorDepth(seaFloorDepth), mAdjustments(std::move(adjustments)) {
}

float LayeredNetherSurface::_adjustmentValue(float frequency, int32_t worldX, int32_t worldZ) const {
    const float x = (float) worldX * frequency;
    const float z = (float) worldZ * frequency;

    float octaveFrequency = 1.0f;
    float total = 0.0f;
    float amplitudeSum = 0.0f;
    for (const SurfaceSimplexNoise &octave: mAdjustmentNoise) {
        total += octave.getValue3D(x * octaveFrequency, 0.0f, z * octaveFrequency) / octaveFrequency;
        amplitudeSum += 1.0f / octaveFrequency;
        octaveFrequency *= 0.5f;
    }

    return total * (1.0f / amplitudeSum);
}

void LayeredNetherSurface::build(const NetherSurfaceColumn &column) const {
    LevelChunk &chunk = column.mChunk;
    const int32_t worldX = (chunk.getX() << 4) + column.mX;
    const int32_t worldZ = (chunk.getZ() << 4) + column.mZ;

    std::vector<const Adjustment *> active;
    for (const Adjustment &adjustment: mAdjustments) {
        const float value = _adjustmentValue(adjustment.mFrequency, worldX, worldZ);
        if (value >= adjustment.mLowerBound && adjustment.mUpperBound > value)
            active.push_back(&adjustment);
    }

    const auto materialsAt = [&](int32_t y, const BlockState *&top, const BlockState *&mid,
                                 const BlockState *&seaFloor) {
        for (const Adjustment *adjustment: active) {
            if (y < adjustment->mMinY || y > adjustment->mMaxY)
                continue;

            if (adjustment->mTop != nullptr)
                top = adjustment->mTop;
            if (adjustment->mMid != nullptr)
                mid = adjustment->mMid;
            if (adjustment->mSeaFloor != nullptr)
                seaFloor = adjustment->mSeaFloor;
        }
    };

    // ranges reaching the top of the column become the materials every height starts from
    const BlockState *baseTop = mTop;
    const BlockState *baseMid = mMid;
    const BlockState *baseSeaFloor = mSeaFloor;
    materialsAt(HEIGHT - 1, baseTop, baseMid, baseSeaFloor);

    const int32_t runDepth = (int32_t) (column.mDepthNoise / 3.0f + 3.0f
                                        + GameRandom::nextFloat(column.mRandom) * 0.25f);
    const int32_t seaFloorDepth = mSeaFloorDepth + runDepth;
    const BlockState *foundation = &netherrackState();

    int32_t remaining = -1;
    const BlockState *layerTop = baseTop;
    const BlockState *layerMid = baseMid;
    for (int32_t y = HEIGHT - 1; y > 0; y--) {
        const BlockState &state = chunk.getBlock(column.mX, y, column.mZ);
        if (sameBlock(state, airState())) {
            remaining = -1;
            continue;
        }

        // bedrock takes the layers like stone: measured against BDS, the roof keeps netherrack in most columns
        const bool bedrock = sameBlock(state, bedrockState());
        if (!bedrock && !sameBlock(state, netherrackState()))
            continue;

        const auto setLayer = [&](const BlockState &layer) {
            if (!bedrock)
                chunk.setBlock(column.mX, y, column.mZ, layer);
        };

        const BlockState *top = baseTop;
        const BlockState *mid = baseMid;
        const BlockState *seaFloor = baseSeaFloor;
        materialsAt(y, top, mid, seaFloor);

        if (remaining == -1) {
            const BlockState *nextMid = foundation;
            const BlockState *nextTop = &airState();
            if (runDepth > 0) {
                const bool outside = y < SEA_LEVEL - 4 || y > SEA_LEVEL + 1;
                nextMid = outside ? layerMid : mid;
                nextTop = outside ? layerTop : top;
            }

            if (y < SEA_LEVEL && sameBlock(*nextTop, airState()))
                nextTop = &lavaState();

            remaining = runDepth;
            if (y >= SEA_LEVEL - 1) {
                setLayer(*nextTop);
            } else if (y < SEA_LEVEL - seaFloorDepth || y == 1) {
                setLayer(*seaFloor);
                layerMid = foundation;
                layerTop = &airState();
                continue;
            } else {
                setLayer(*nextMid);
            }

            layerMid = nextMid;
            layerTop = nextTop;
            continue;
        }

        if (remaining <= 0) {
            setLayer(*foundation);
            continue;
        }

        setLayer(*layerMid);
        remaining--;
    }
}

NetherSurfaceBuilder::NetherSurfaceBuilder(uint32_t seed, const std::vector<SurfaceSimplexNoise> &depthNoise,
                                           const std::vector<SurfaceSimplexNoise> &adjustmentNoise)
        : mDepthNoise(depthNoise) {
    std::mt19937 beachRandom(seed);
    mBeachNoise = std::make_unique<TheEndOctaveNoise>(beachRandom, 1);

    mSurfaces[NetherBiomeIds::SOULSAND_VALLEY] = std::make_unique<CappedNetherSurface>(
            seed, std::vector<const BlockState *>{&soulSandState(), &soulSoilState()},
            std::vector<const BlockState *>{&soulSandState(), &soulSoilState()}, &gravelState(), *mBeachNoise);
    mSurfaces[NetherBiomeIds::BASALT_DELTAS] = std::make_unique<CappedNetherSurface>(
            seed, std::vector<const BlockState *>{&basaltState(), &blackstoneState()},
            std::vector<const BlockState *>{&basaltState()}, nullptr, *mBeachNoise);

    mSurfaces[NetherBiomeIds::HELL] = std::make_unique<LayeredNetherSurface>(
            adjustmentNoise, netherrackState(), netherrackState(), netherrackState(), 7,
            std::vector<LayeredNetherSurface::Adjustment>{
                    {0.03125f, -1.0f, -0.4f, SEA_LEVEL - 3, SEA_LEVEL + 2, &gravelState(), nullptr, &gravelState()},
                    {0.03125f, 0.4f, 1.0f, SEA_LEVEL - 3, SEA_LEVEL + 2, &soulSandState(), &soulSandState(),
                     &soulSandState()}});
    mSurfaces[NetherBiomeIds::CRIMSON_FOREST] = std::make_unique<LayeredNetherSurface>(
            adjustmentNoise, crimsonNyliumState(), netherrackState(), crimsonNyliumState(), 1,
            std::vector<LayeredNetherSurface::Adjustment>{
                    {0.1f, 0.4f, 1.0f, 0, HEIGHT - 1, &netherrackState(), nullptr, &netherrackState()},
                    {1.0f, -0.75f, -0.5f, 0, HEIGHT - 1, &netherWartState(), nullptr, &netherWartState()}});
    mSurfaces[NetherBiomeIds::WARPED_FOREST] = std::make_unique<LayeredNetherSurface>(
            adjustmentNoise, warpedNyliumState(), netherrackState(), warpedNyliumState(), 1,
            std::vector<LayeredNetherSurface::Adjustment>{
                    {0.1f, 0.4f, 1.0f, 0, HEIGHT - 1, &netherrackState(), nullptr, &netherrackState()}});
}

void NetherSurfaceBuilder::_fillDepthNoise(int32_t chunkX, int32_t chunkZ, float *noise) const {
    std::fill(noise, noise + 256, 0.0f);

    const float baseX = (float) (chunkX * 16);
    const float baseZ = (float) (chunkZ * 16);
    float amplitude = 1.0f;
    for (const SurfaceSimplexNoise &octave: mDepthNoise) {
        const float scale = amplitude * 0.0625f;
        const float weight = 0.55f / amplitude;
        for (int32_t z = 0; z < 16; z++) {
            const float sampleZ = ((float) z + baseZ) * scale + octave.getOffsetY();
            for (int32_t x = 0; x < 16; x++) {
                const float sampleX = ((float) x + baseX) * scale + octave.getOffsetX();
                noise[x + 16 * z] = octave.getValue(sampleX, sampleZ) * weight + noise[x + 16 * z];
            }
        }
        amplitude *= 0.5f;
    }
}

void NetherSurfaceBuilder::apply(LevelChunk &chunk) const {
    const uint32_t chunkSeed = (uint32_t) -245998635 * (uint32_t) chunk.getZ()
                               - (uint32_t) 1724254968 * (uint32_t) chunk.getX();
    std::mt19937 random(chunkSeed);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t top = GameRandom::nextInt(random, 4) + 1;
            for (int32_t y = 0; y <= top; y++)
                chunk.setBlock(x, y, z, bedrockState());
        }
    }

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t depth = GameRandom::nextInt(random, 5);
            for (int32_t y = HEIGHT - 1 - depth; y < HEIGHT; y++)
                chunk.setBlock(x, y, z, bedrockState());
        }
    }

    float depthNoise[256];
    _fillDepthNoise(chunk.getX(), chunk.getZ(), depthNoise);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const auto surface = mSurfaces.find((int32_t) chunk.getBiomeAt(x, 0, z));
            if (surface == mSurfaces.end())
                continue;

            surface->second->build({chunk, random, x, z, depthNoise[x + 16 * z]});
        }
    }
}
