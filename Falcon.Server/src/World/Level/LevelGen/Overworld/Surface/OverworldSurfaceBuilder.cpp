#include "World/Level/LevelGen/Overworld/Surface/OverworldSurfaceBuilder.h"

#include "World/Level/Block/LiquidBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

namespace {
    // octaves of minecraft:surface built in sub_14CDE1490
    constexpr int32_t SURFACE_NOISE_OCTAVES = 4;

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

    // a mid material that turns to sandstone where its layer ends (list at 0x1529B0DF8, filled from 0x14F1F6538)
    bool isSand(const BlockState &state) {
        static const BlockStateSet SAND = {
                VanillaBlocks::SAND().getIdentifier(), VanillaBlocks::RED_SAND().getIdentifier(),
                VanillaBlocks::SUSPICIOUS_SAND().getIdentifier()
        };
        return SAND.contains(state);
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

OverworldSurfaceBuilder::OverworldSurfaceBuilder(const XoroshiroPositionalFactory &noiseFactory,
                                                 const OverworldClimate &climate)
        : mClimate(climate) {
    XoroshiroRandom random = noiseFactory.hashOf("minecraft:surface");
    mSurfaceNoise.reserve(SURFACE_NOISE_OCTAVES);
    for (int32_t i = 0; i < SURFACE_NOISE_OCTAVES; i++)
        mSurfaceNoise.emplace_back(random);

    mNoise = NormalNoise::create(noiseFactory, "minecraft:surface", -6, {1.0f, 1.0f, 1.0f});
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
            const SurfaceBuilderComponent *component = SurfaceBuilderComponent::getForBiome(
                    (int32_t) chunk.getBiomeAt(x, std::min(height, LevelChunk::MAX_Y), z));
            if (component == nullptr)
                continue;

            if (component->mType == SurfaceBuilderType::Overworld)
                _buildOverworldColumn(chunk, x, z, height, *component, noise[x + 16 * z], random);
            else
                _buildLegacyColumn(chunk, x, z, *component);
        }
    }
}

void OverworldSurfaceBuilder::_adjustMaterials(const SurfaceBuilderComponent &component, int32_t worldX, int32_t worldZ,
                                               const BlockState *&top, const BlockState *&mid,
                                               const BlockState *&seaFloor) const {
    if (component.mSurfaceMaterialAdjustments.empty())
        return;

    const float value = mNoise->getValue(worldX, 0.0, worldZ);
    for (const SurfaceMaterialAdjustmentAttributes &element: component.mSurfaceMaterialAdjustments) {
        if (value >= element.mNoiseUpperBound || value <= element.mNoiseLowerBound)
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

    // nothing more than 8 blocks under the column's preliminary surface is touched (sub_14A22FC50)
    const int32_t preliminary = static_cast<int16_t>(mClimate.preliminarySurfaceLevel(worldX >> 2, worldZ >> 2) - 8);
    int32_t lowest = LevelChunk::MAX_Y;
    if (preliminary <= LevelChunk::MAX_Y)
        lowest = std::max(preliminary, LevelChunk::MIN_Y);

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

        if (isSand(*layerMid)) {
            const int32_t extra = static_cast<int32_t>(random() % 4u);
            remaining = static_cast<int32_t>(std::max<int64_t>(0, (int64_t) y - seaLevel)) + extra;
            layerMid = &sandstoneState();
        }
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
