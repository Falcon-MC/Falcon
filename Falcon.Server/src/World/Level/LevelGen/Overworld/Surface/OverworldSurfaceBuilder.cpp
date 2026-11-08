#include "World/Level/LevelGen/Overworld/Surface/OverworldSurfaceBuilder.h"

#include "World/Level/Block/LiquidBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/LevelGen/Overworld/OverworldBlockLists.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {
    // octaves of minecraft:surface built in sub_14CDE1490
    constexpr int32_t SURFACE_NOISE_OCTAVES = 4;

    // the surface_material_adjustments stack of sub_14CDDFC50 and the draws its twister made before it
    constexpr int32_t ADJUSTMENT_NOISE_OCTAVES = 3;
    constexpr unsigned long long ADJUSTMENT_NOISE_DRAWS = 20202;

    // the swamp builder's stack (sub_1471363F0) and its sampling scale (sub_147136880)
    constexpr int32_t SWAMP_NOISE_OCTAVES = 2;
    constexpr float SWAMP_NOISE_SCALE = 0.25f;
    constexpr int32_t SWAMP_SEA_LEVEL = 63;

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &iceState() {
        static const BlockState state = VanillaBlocks::ICE().toBlockState();
        return state;
    }

    const BlockState &sandstoneState() {
        static const BlockState state = VanillaBlocks::SANDSTONE().toBlockState();
        return state;
    }

    bool isWaterState(const BlockState &state) {
        return state.getHash() == waterState().getHash();
    }

    const BlockState &redSandstoneState() {
        static const BlockState state = VanillaBlocks::RED_SANDSTONE().toBlockState();
        return state;
    }

    bool isLiquid(const BlockState &state) {
        static const BlockStateSet LIQUIDS = BlockStateSet::ofBlocks([](const Block &block) {
            const LiquidBlock *liquid = dynamic_cast<const LiquidBlock *>(&block);
            return liquid != nullptr && liquid->isLiquid();
        });
        return LIQUIDS.contains(state);
    }

    // the game's starting value for the column's liquid top, far below any height (0x800003E8 in sub_14A22BF80)
    constexpr int32_t NO_LIQUID = INT32_MIN + 1000;

    // the only blocks the builder replaces (list at 0x1529AB1B8, filled from 0x14F1F5B40)
    bool isReplaceable(const BlockState &state) {
        static const BlockStateSet REPLACEABLE = {
                VanillaBlocks::STONE().getIdentifier(), VanillaBlocks::GRANITE().getIdentifier(),
                VanillaBlocks::POLISHED_GRANITE().getIdentifier(), VanillaBlocks::DIORITE().getIdentifier(),
                VanillaBlocks::POLISHED_DIORITE().getIdentifier(), VanillaBlocks::ANDESITE().getIdentifier(),
                VanillaBlocks::POLISHED_ANDESITE().getIdentifier()
        };
        return REPLACEABLE.contains(state);
    }

    bool isAirState(const BlockState &state) {
        return state.getHash() == airState().getHash();
    }

    float remap(float input, float inMin, float inMax, float outMin, float outMax) {
        return outMin + ((input - inMin) / (inMax - inMin) * (outMax - outMin));
    }

    float remapFromNormalized(float input, float outMin, float outMax) {
        return remap(input, -1.0f, 1.0f, outMin, outMax);
    }
}

namespace {
    // sub_1471334C0 takes the seed as an int and sign-extends it
    XoroshiroPositionalFactory icebergFactory(int64_t levelSeed) {
        XoroshiroRandom random(static_cast<int64_t>(static_cast<int32_t>(levelSeed)));
        return random.forkPositional();
    }
}

OverworldSurfaceBuilder::OverworldSurfaceBuilder(const XoroshiroPositionalFactory &noiseFactory,
                                                 const OverworldClimate &climate, int64_t levelSeed)
        : mClimate(climate), mIcebergFactory(icebergFactory(levelSeed)),
          mSulfurCaves(NoiseGradientSurfaceBuilder::sulfurCaves(levelSeed)) {
    XoroshiroRandom random = noiseFactory.hashOf("minecraft:surface");
    mSurfaceNoise.reserve(SURFACE_NOISE_OCTAVES);
    for (int32_t i = 0; i < SURFACE_NOISE_OCTAVES; i++)
        mSurfaceNoise.emplace_back(random);

    mNoise = NormalNoise::create(noiseFactory, "minecraft:surface", -6, {1.0f, 1.0f, 1.0f});

    // the generator's adjustment stack (ctor sub_14CDDFC50, +3408): its twister has drawn for every stack before it
    std::mt19937 adjustmentRandom(static_cast<uint32_t>(levelSeed));
    adjustmentRandom.discard(ADJUSTMENT_NOISE_DRAWS);
    for (int32_t i = 0; i < ADJUSTMENT_NOISE_OCTAVES; i++)
        mAdjustmentNoise.emplace_back(adjustmentRandom);

    std::mt19937 swampRandom(static_cast<uint32_t>(levelSeed));
    for (int32_t i = 0; i < SWAMP_NOISE_OCTAVES; i++)
        mSwampNoise.emplace_back(swampRandom);

    // the frozen ocean builder's noises (sub_1471334C0)
    mIcebergPillarNoise = NormalNoise::create(mIcebergFactory, "minecraft:iceberg_pillar", -6, {1.0f, 1.0f, 1.0f, 1.0f});
    mIcebergPillarRoofNoise = NormalNoise::create(mIcebergFactory, "minecraft:iceberg_pillar_roof", -3, {1.0f});
    mIcebergSurfaceNoise = NormalNoise::create(mIcebergFactory, "minecraft:iceberg_surface", -6, {1.0f, 1.0f, 1.0f});
}

// per column x + 16 * z, in sub_14CDD16A0
void OverworldSurfaceBuilder::_fillSurfaceNoise(int32_t chunkX, int32_t chunkZ, float *noise) const {
    std::fill(noise, noise + 256, 0.0f);

    const float baseX = static_cast<float>(chunkX << 4);
    const float baseZ = static_cast<float>(chunkZ << 4);
    float amplitude = 1.0f;
    for (const SurfaceSimplexNoise &octave: mSurfaceNoise) {
        const float scale = amplitude * 0.125f;
        const float weight = 0.55f / amplitude;
        for (int32_t z = 0; z < 16; z++) {
            const float sampleZ = (static_cast<float>(z) + baseZ) * scale + octave.getOffsetY();
            for (int32_t x = 0; x < 16; x++) {
                const float sampleX = (static_cast<float>(x) + baseX) * scale + octave.getOffsetX();
                noise[x + 16 * z] = octave.getValue(sampleX, sampleZ) * weight + noise[x + 16 * z];
            }
        }
        amplitude *= 0.5f;
    }
}

void OverworldSurfaceBuilder::apply(LevelChunk &chunk, std::mt19937 &random) const {
    float noise[256];
    _fillSurfaceNoise(chunk.getX(), chunk.getZ(), noise);

    // columns in the order of sub_14A22FC50, each with the biome just above its highest block (sub_14A22F190)
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            int32_t top = LevelChunk::MAX_Y;
            while (top >= LevelChunk::MIN_Y && isAirState(chunk.getBlock(x, top, z)))
                top--;

            const int32_t height = std::max(0, top + 1);
            const int32_t biome = (int32_t) chunk.getBiomeAt(x, std::min(height, LevelChunk::MAX_Y), z);
            // the volume is 320 blocks tall and the builder leaves its top layer
            const NoiseGradientSurfaceBuilder *gradient = _noiseGradientFor(biome);
            if (gradient != nullptr) {
                gradient->build(chunk, x, z, _lowestSurfaceY((chunk.getX() << 4) + x, (chunk.getZ() << 4) + z),
                                LevelChunk::MIN_Y + 319);
                continue;
            }

            const SurfaceBuilderComponent *component = SurfaceBuilderComponent::getForBiome(biome);
            if (component == nullptr)
                continue;

            // the frozen ocean builder (vtable 0x14F2EF500) raises its ice, then builds like the overworld one
            if (component->mType == SurfaceBuilderType::FrozenOcean)
                _buildIcebergColumn(chunk, x, z, height);
            // the swamp builder (sub_147136880) floods its puddle, then builds like the overworld one
            if (component->mType == SurfaceBuilderType::Swamp)
                _digSwampPuddle(chunk, x, z, height, component->mMaxPuddleDepth);
            if (component->mType == SurfaceBuilderType::Overworld || component->mType == SurfaceBuilderType::FrozenOcean
                || component->mType == SurfaceBuilderType::Swamp)
                _buildOverworldColumn(chunk, x, z, height, *component, noise[x + 16 * z], random);
            else
                _buildLegacyColumn(chunk, x, z, *component);
        }
    }

    _buildSubsurface(chunk);
}

const NoiseGradientSurfaceBuilder *OverworldSurfaceBuilder::_noiseGradientFor(int32_t biome) const {
    return biome == BiomeIds::SULFUR_CAVES ? &mSulfurCaves : nullptr;
}

// sub_14CDD16A0: runs of one biome up to the lowest surface block; a run's top block and a last lone block are left
void OverworldSurfaceBuilder::_buildSubsurface(LevelChunk &chunk) const {
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t highest = _lowestSurfaceY((chunk.getX() << 4) + x, (chunk.getZ() << 4) + z);
            int32_t runBiome = (int32_t) chunk.getBiomeAt(x, LevelChunk::MIN_Y, z);
            int32_t runStart = LevelChunk::MIN_Y;
            int32_t runEnd = LevelChunk::MIN_Y;
            for (int32_t y = LevelChunk::MIN_Y; y <= highest; y++) {
                const int32_t biome = (int32_t) chunk.getBiomeAt(x, y, z);
                if (biome == runBiome) {
                    runEnd = y;
                    if (y != highest)
                        continue;
                }

                const NoiseGradientSurfaceBuilder *gradient = _noiseGradientFor(runBiome);
                if (gradient != nullptr)
                    gradient->build(chunk, x, z, runStart, runEnd);

                runBiome = biome;
                runStart = y;
                runEnd = y;
            }
        }
    }
}

void OverworldSurfaceBuilder::_adjustMaterials(const SurfaceBuilderComponent &component, int32_t worldX, int32_t worldZ,
                                               const BlockState *&top, const BlockState *&mid,
                                               const BlockState *&seaFloor) const {
    if (component.mSurfaceMaterialAdjustments.empty())
        return;

    // sub_145D43EF0: one 3D simplex sum per adjustment at its own frequency, the low bound included
    for (const SurfaceMaterialAdjustmentAttributes &element: component.mSurfaceMaterialAdjustments) {
        float value = 0.0f;
        float amplitude = 1.0f;
        float amplitudeSum = 0.0f;
        const float x = static_cast<float>(worldX) * element.mNoiseFrequencyScale;
        const float z = static_cast<float>(worldZ) * element.mNoiseFrequencyScale;
        for (const SurfaceSimplexNoise &octave: mAdjustmentNoise) {
            value += octave.getValue3D(x * amplitude, 0.0f, z * amplitude) / amplitude;
            amplitudeSum += 1.0f / amplitude;
            amplitude *= 0.5f;
        }
        value *= 1.0f / amplitudeSum;

        if (value < element.mNoiseLowerBound || value >= element.mNoiseUpperBound)
            continue;

        if (element.mTopBlock != nullptr)
            top = element.mTopBlock;

        if (element.mMidBlock != nullptr)
            mid = element.mMidBlock;

        if (element.mSeaFloorBlock != nullptr)
            seaFloor = element.mSeaFloorBlock;
    }
}

// the game's overworld builder (sub_14A22BF80), in absolute heights
void OverworldSurfaceBuilder::_buildOverworldColumn(LevelChunk &chunk, int32_t x, int32_t z, int32_t height,
                                                    const SurfaceBuilderComponent &component, float noise,
                                                    std::mt19937 &random) const {
    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    const SurfaceMaterialAttributes &material = component.mSurfaceMaterial;
    const BlockState *top = material.mTopBlock;
    const BlockState *mid = material.mMidBlock;
    const BlockState *seaFloor = material.mSeaFloorBlock;
    _adjustMaterials(component, worldX, worldZ, top, mid, seaFloor);
    const BlockState *foundation = material.mFoundationBlock;
    const BlockState *sea = material.mSeaBlock;

    const ClimateAttributes *climate = ClimateAttributes::getForBiome(
            (int32_t) chunk.getBiomeAt(x, std::min(height, LevelChunk::MAX_Y), z));
    const float temperature = climate != nullptr ? climate->mTemperature : 0.5f;

    const int32_t runDepth = static_cast<int32_t>((noise / 3.0f + 3.0f) + GameRandom::nextFloat(random) * 0.25f);
    const int32_t seaFloorDepth = material.mSeaFloorDepth + runDepth;

    const int32_t lowest = _lowestSurfaceY(worldX, worldZ);

    // the game's sea level here is the top of the liquid met since the last air, not the dimension's
    int32_t seaLevel = NO_LIQUID;
    int32_t remaining = -1;
    const BlockState *layerTop = top;
    const BlockState *layerMid = mid;
    static const BlockStateSet BEDROCK = {VanillaBlocks::BEDROCK().getIdentifier()};
    static const BlockStateSet NETHERRACK = {VanillaBlocks::NETHERRACK().getIdentifier()};
    static const BlockStateSet SAND = {VanillaBlocks::SAND().getIdentifier()};
    static const BlockStateSet RED_SAND = {VanillaBlocks::RED_SAND().getIdentifier()};
    static const BlockStateSet GRAVEL = {VanillaBlocks::GRAVEL().getIdentifier()};
    static const BlockStateSet SEA_ICE = {VanillaBlocks::PACKED_ICE().getIdentifier(),
                                          VanillaBlocks::BLUE_ICE().getIdentifier()};
    for (int32_t y = std::min(height, LevelChunk::MAX_Y); y > lowest; y--) {
        const BlockState &state = chunk.getBlock(x, y, z);
        if (BEDROCK.contains(state))
            continue;

        // a sand or gravel layer left over a cave or a liquid would float: harden it
        if (remaining >= 0 && !NETHERRACK.contains(*foundation) && (isAirState(state) || isLiquid(state))
            && y < LevelChunk::MAX_Y) {
            const BlockState &above = chunk.getBlock(x, y + 1, z);
            if (SAND.contains(above))
                chunk.setBlock(x, y + 1, z, sandstoneState());
            else if (RED_SAND.contains(above))
                chunk.setBlock(x, y + 1, z, redSandstoneState());
            else if (GRAVEL.contains(above))
                chunk.setBlock(x, y + 1, z, *foundation);
        }

        if (isAirState(state)) {
            remaining = -1;
            seaLevel = NO_LIQUID;
            continue;
        }

        if (!isReplaceable(state)) {
            if (isWaterState(state) && sea != &waterState())
                chunk.setBlock(x, y, z, *sea);
            if (isLiquid(state) || SEA_ICE.contains(state))
                seaLevel = std::max(seaLevel, y + 1);
            continue;
        }

        if (remaining == -1) {
            const BlockState *nextMid = foundation;
            const BlockState *nextTop = &airState();
            if (runDepth > 0) {
                const bool outside = y < seaLevel - 4 || y > seaLevel + 1;
                nextMid = outside ? layerMid : mid;
                nextTop = outside ? layerTop : top;
            }

            if (y < seaLevel && isAirState(*nextTop))
                nextTop = isWaterState(*sea) && temperature < 0.15f ? &iceState() : sea;

            remaining = runDepth;
            if (y >= seaLevel - 1) {
                chunk.setBlock(x, y, z, *nextTop);
            } else if (y < seaLevel - seaFloorDepth || y == lowest + 1) {
                chunk.setBlock(x, y, z, *seaFloor);
                layerMid = foundation;
                layerTop = &airState();
                continue;
            } else {
                chunk.setBlock(x, y, z, *nextMid);
            }

            layerMid = nextMid;
            layerTop = nextTop;
            continue;
        }

        if (remaining <= 0) {
            chunk.setBlock(x, y, z, *foundation);
            continue;
        }

        chunk.setBlock(x, y, z, *layerMid);
        if (--remaining != 0)
            continue;

        // a mid material that turns to sandstone where its layer ends
        if (OverworldBlockLists::sand().contains(*layerMid)) {
            const int32_t extra = static_cast<int32_t>(random() % 4u);
            remaining = static_cast<int32_t>(std::max<int64_t>(0, (int64_t) y - seaLevel)) + extra;
            layerMid = &sandstoneState();
        }
    }
}

int32_t OverworldSurfaceBuilder::_lowestSurfaceY(int32_t worldX, int32_t worldZ) const {
    // nothing more than 8 blocks under the column's preliminary surface is touched
    const int32_t preliminary = static_cast<int16_t>(mClimate.preliminarySurfaceLevel(worldX >> 2, worldZ >> 2) - 8);
    if (preliminary > LevelChunk::MAX_Y)
        return LevelChunk::MAX_Y;
    return std::max(preliminary, LevelChunk::MIN_Y);
}

void OverworldSurfaceBuilder::_digSwampPuddle(LevelChunk &chunk, int32_t x, int32_t z, int32_t height,
                                              int32_t maxDepth) const {
    const auto worldX = static_cast<float>((chunk.getX() << 4) + x);
    const auto worldZ = static_cast<float>((chunk.getZ() << 4) + z);
    float value = 0.0f;
    float amplitude = 1.0f;
    for (const SurfaceSimplexNoise &octave: mSwampNoise) {
        value += octave.getValue(worldX * SWAMP_NOISE_SCALE * amplitude, worldZ * SWAMP_NOISE_SCALE * amplitude)
                 / amplitude;
        amplitude *= 0.5f;
    }

    const int32_t lowest = SWAMP_SEA_LEVEL - maxDepth;
    if (value <= 0.0f || height > SWAMP_SEA_LEVEL || lowest >= height)
        return;

    // the highest block that is not water yet, down to the puddle's depth
    for (int32_t y = height - 1; y >= lowest; y--) {
        if (!isWaterState(chunk.getBlock(x, y, z))) {
            chunk.setBlock(x, y, z, waterState());
            return;
        }
    }
}

void OverworldSurfaceBuilder::_buildIcebergColumn(LevelChunk &chunk, int32_t x, int32_t z, int32_t height) const {
    // sub_147135EC0, in floats: a pillar where the noises rise above 1.8, packed ice into the air above the sea and
    // into the water a little under it, a few snow blocks on top, each block drawn from the column's own random
    constexpr int32_t SEA_LEVEL = 63;
    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    const auto blockX = static_cast<float>(worldX);
    const auto blockZ = static_cast<float>(worldZ);
    const float pillar = mIcebergPillarNoise->getValue(1.28f * blockX, 0.0, blockZ * 1.28f) * 15.0f;
    float iceberg = std::fmin(std::fabs(mIcebergSurfaceNoise->getValue(blockX, 0.0, blockZ) * 8.25f), pillar);
    if (!(iceberg > 1.8f))
        return;

    const float roof = std::fabs(mIcebergPillarRoofNoise->getValue(blockX * 1.17f, 0.0, blockZ * 1.17f) * 1.5f) * 40.0f;
    const auto roofTruncated = static_cast<int32_t>(roof);
    const int32_t roofCeiling = static_cast<float>(roofTruncated) < roof ? roofTruncated + 1 : roofTruncated;
    iceberg = std::fmin(iceberg * iceberg * 1.2f, static_cast<float>(roofCeiling + 14.0));

    // shouldMeltFrozenOceanIcebergSlightly: the temperature at sea level
    const auto biome = static_cast<int32_t>(chunk.getBiomeAt(x, SEA_LEVEL, z));
    if (0.1f < BiomeTemperature::at(biome, worldX, SEA_LEVEL, worldZ))
        iceberg = iceberg + -2.0f;

    const auto sea = static_cast<float>(SEA_LEVEL);
    const float bottom = iceberg > 2.0f ? (sea - iceberg) + -7.0f : 0.0f;
    const float top = 2.0f < iceberg ? sea + iceberg : 0.0f;

    XoroshiroRandom random = mIcebergFactory.at(worldX, 0, worldZ);
    const int32_t maxSnow = random.nextExclusiveInt(4) + 2;
    const int32_t snowLine = SEA_LEVEL + 18 + random.nextExclusiveInt(10);

    static const BlockState packedIce = VanillaBlocks::PACKED_ICE().toBlockState();
    static const BlockState snow = VanillaBlocks::SNOW().toBlockState();
    static const BlockStateSet WATER = {VanillaBlocks::WATER().getIdentifier(),
                                        VanillaBlocks::FLOWING_WATER().getIdentifier()};
    const int32_t lowest = _lowestSurfaceY(worldX, worldZ);
    auto y = static_cast<int32_t>(std::fmax(static_cast<float>(height), top + 1.0f));
    if (lowest > y)
        return;

    int32_t snowPlaced = 0;
    for (;; y--) {
        const BlockState &state = chunk.getBlock(x, y, z);
        bool place = false;
        if (isAirState(state) && y < static_cast<int32_t>(top))
            place = random.nextFloat() > 0.01f;
        else if (WATER.contains(state) && y > static_cast<int32_t>(bottom) && y < SEA_LEVEL && bottom != 0.0f)
            place = random.nextFloat() > 0.15f;

        if (place) {
            if (snowPlaced > maxSnow || y <= snowLine) {
                chunk.setBlock(x, y, z, packedIce);
            } else {
                chunk.setBlock(x, y, z, snow);
                snowPlaced++;
            }
        }

        if (y <= lowest)
            break;
    }
}

void OverworldSurfaceBuilder::_buildLegacyColumn(LevelChunk &chunk, int32_t x, int32_t z,
                                                 const SurfaceBuilderComponent &component) const {
    const int32_t y = chunk.getHeight(x, z);
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    const BlockState topBlockState = chunk.getBlock(x, y, z);
    const SurfaceMaterialAttributes &material = component.mSurfaceMaterial;
    const BlockState *topBlock = material.mTopBlock;
    const BlockState *midBlock = material.mMidBlock;
    const BlockState *seaFloorBlock = material.mSeaFloorBlock;
    _adjustMaterials(component, (chunk.getX() << 4) + x, (chunk.getZ() << 4) + z, topBlock, midBlock, seaFloorBlock);

    if (!isWaterState(topBlockState)) {
        chunk.setBlock(x, y, z, *topBlock);

        const float midDepth = remapFromNormalized(mNoise->getValue(x, 0.0, z), 1.0f, 4.0f);
        for (int32_t i = 1; i < midDepth; i++) {
            const int32_t midY = y - i;
            if (midY < LevelChunk::MIN_Y)
                break;

            chunk.setBlock(x, midY, z, *midBlock);
        }
        return;
    }

    int32_t depth = 0;
    BlockState currentState = topBlockState;
    while (isWaterState(currentState)) {
        depth++;
        if (y - depth < LevelChunk::MIN_Y)
            break;

        currentState = chunk.getBlock(x, y - depth, z);
    }

    if (y - depth < LevelChunk::MIN_Y)
        return;

    if (depth > material.mSeaFloorDepth) {
        chunk.setBlock(x, y - depth, z, *seaFloorBlock);
        return;
    }

    for (int32_t i = 0; i < 3; i++) {
        const int32_t midY = (y - depth) - i;
        if (midY < LevelChunk::MIN_Y)
            break;

        chunk.setBlock(x, midY, z, *midBlock);
    }
}
