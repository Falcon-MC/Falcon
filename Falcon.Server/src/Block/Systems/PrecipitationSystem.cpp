#include "Block/Systems/PrecipitationSystem.h"

#include "Block/Blocks/GrowthBlocks.h"
#include "Block/Blocks/IceBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Biome/ClimateAttributes.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const int PRECIPITATION_CHANCE = 16;
    const int MAX_FREEZE_LIGHT = 10;
    const float COLD_TEMPERATURE = 0.15f;
    const int32_t TEMPERATURE_FALLOFF_START = 80;
    const float TEMPERATURE_FALLOFF_PER_BLOCK = 0.05f / 40.0f;

    const Vector3i HORIZONTAL_OFFSETS[] = {
            Vector3i(0, 0, -1), Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    const BlockState *stateAt(Level &level, const Vector3i &position) {
        return level.peekBlockPtr(position.x, position.y, position.z);
    }

    bool isWaterSource(const BlockState *state) {
        return state != nullptr && WaterBlock::matches(state->mName) && state->mStates.getInt("liquid_depth", 0) == 0;
    }

    bool isWater(const BlockState *state) {
        return state != nullptr && WaterBlock::matches(state->mName);
    }

    bool isInRange(Level &level, const Vector3i &position) {
        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }
}

void PrecipitationSystem::tick(ServerNetworkHandler &owner, Level &level) {
    (void) owner;

    if (!level.hasSkyLight())
        return;

    for (const int64_t column: level.getActiveColumns()) {
        if (RandomTickSystem::nextInt(PRECIPITATION_CHANCE) != 0)
            continue;

        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        if (level.peekChunkPtr(chunkX, chunkZ) == nullptr)
            continue;

        tickColumn(level, (chunkX << 4) + RandomTickSystem::nextInt(16), (chunkZ << 4) + RandomTickSystem::nextInt(16));
    }
}

bool PrecipitationSystem::isCold(Level &level, const Vector3i &position) {
    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr)
        return false;

    const ClimateAttributes *climate = ClimateAttributes::getForBiome(
            (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15));
    if (climate == nullptr)
        return false;

    float temperature = climate->mTemperature;
    if (position.y > TEMPERATURE_FALLOFF_START)
        temperature -= (float) (position.y - TEMPERATURE_FALLOFF_START) * TEMPERATURE_FALLOFF_PER_BLOCK;

    return temperature < COLD_TEMPERATURE;
}

void PrecipitationSystem::tickColumn(Level &level, int32_t x, int32_t z) {
    int32_t surface = level.getMaxY();
    while (surface >= level.getMinY()) {
        const BlockState *state = level.peekBlockPtr(x, surface, z);
        if (state == nullptr)
            return;
        if (state->mName != "minecraft:air")
            break;
        --surface;
    }

    if (surface < level.getMinY())
        return;

    const Vector3i below(x, surface, z);
    const Vector3i top(x, surface + 1, z);

    if (shouldFreeze(level, below))
        level.setBlock(below, BlockState(IceBlock::IDENTIFIER), true);

    if (level.isRaining() && level.canRainAt(x, z) && shouldSnow(level, top))
        level.setBlock(top, VanillaBlocks::SNOW_LAYER().toBlockState(), true);
}

bool PrecipitationSystem::shouldFreeze(Level &level, const Vector3i &position) {
    if (!isInRange(level, position) || !isCold(level, position))
        return false;

    if (RandomTickSystem::getBlockLight(level, position) >= MAX_FREEZE_LIGHT)
        return false;

    if (!isWaterSource(stateAt(level, position)))
        return false;

    for (const Vector3i &offset: HORIZONTAL_OFFSETS) {
        const Vector3i side(position.x + offset.x, position.y, position.z + offset.z);
        const BlockState *neighbour = stateAt(level, side);
        if (neighbour != nullptr && !isWater(neighbour))
            return true;
    }

    return false;
}

bool PrecipitationSystem::shouldSnow(Level &level, const Vector3i &position) {
    if (!isInRange(level, position) || !isCold(level, position))
        return false;

    if (RandomTickSystem::getBlockLight(level, position) >= MAX_FREEZE_LIGHT)
        return false;

    const BlockState *state = stateAt(level, position);
    if (state == nullptr || state->mName != "minecraft:air")
        return false;

    const Vector3i below(position.x, position.y - 1, position.z);
    const BlockState *support = stateAt(level, below);
    if (support == nullptr || IceBlock::matches(support->mName) || support->mName == "minecraft:packed_ice")
        return false;

    return level.isSolidAt(below.x, below.y, below.z) || VanillaBlocks::getAs<LeavesBlock>(support->mName) != nullptr;
}
