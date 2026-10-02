#include "Block/Blocks/FarmlandBlock.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/CropBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <random>

FALCON_REGISTER_BLOCK(FarmlandBlock, 245);

namespace {
    const char *MOISTURE_STATE = "moisturized_amount";
    const int32_t MAX_MOISTURE = 7;
    const int32_t WATER_SEARCH_RADIUS = 4;
    const float TRAMPLE_FALL_OFFSET = 0.5f;
    const float TRAMPLE_MIN_VOLUME = 0.512f;

    bool isWater(const BlockState &state) {
        return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water";
    }

    float nextFloat() {
        static std::mt19937 generator{std::random_device{}()};
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        return distribution(generator);
    }

    bool canTrample(Level &level, Actor &actor) {
        if (dynamic_cast<ServerPlayer *>(&actor) != nullptr)
            return true;

        const MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob == nullptr || !level.getGameRules().getBool("mobgriefing"))
            return false;

        const ActorSize size = mob->getSize();
        return size.mWidth * size.mWidth * size.mHeight > TRAMPLE_MIN_VOLUME;
    }
}

bool FarmlandBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void FarmlandBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) const {
    (void) owner;

    const int32_t moisture = state.mStates.getInt(MOISTURE_STATE, 0);

    if (isHydrated(level, position)) {
        if (moisture < MAX_MOISTURE)
            level.setBlock(position, DecorationSupport::withState(state, MOISTURE_STATE, MAX_MOISTURE), false);
        return;
    }

    if (moisture > 0) {
        level.setBlock(position, DecorationSupport::withState(state, MOISTURE_STATE, moisture - 1), false);
        return;
    }

    if (!maintainsFarmland(level, position))
        BlockChangeSystem::change(level, position, BlockState("minecraft:dirt"), BlockChangeCause::Fade, true);
}

void FarmlandBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;
    (void) state;

    if (level.isSolidAt(position.x, position.y + 1, position.z))
        turnToDirt(level, position);
}

void FarmlandBlock::onFallOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                             const BlockState &state, float fallDistance) const {
    (void) state;

    Level &level = owner.getLevelFor(actor);
    if (nextFloat() >= fallDistance - TRAMPLE_FALL_OFFSET || !canTrample(level, actor))
        return;

    turnToDirt(level, position);
}

bool FarmlandBlock::isHydrated(Level &level, const Vector3i &position) {
    return isNearWater(level, position) || isRainingAbove(level, position);
}

bool FarmlandBlock::isNearWater(Level &level, const Vector3i &position) {
    for (int32_t y = position.y; y <= position.y + 1; ++y) {
        for (int32_t x = position.x - WATER_SEARCH_RADIUS; x <= position.x + WATER_SEARCH_RADIUS; ++x) {
            for (int32_t z = position.z - WATER_SEARCH_RADIUS; z <= position.z + WATER_SEARCH_RADIUS; ++z) {
                const BlockState *base = level.peekBlockPtr(x, y, z);
                const BlockState *liquid = level.peekBlockPtr(x, y, z, 1);
                if ((base != nullptr && isWater(*base)) || (liquid != nullptr && isWater(*liquid)))
                    return true;
            }
        }
    }

    return false;
}

bool FarmlandBlock::isRainingAbove(Level &level, const Vector3i &position) {
    if (!level.hasSkyLight() || !level.isRaining() || !level.canRainAt(position.x, position.z))
        return false;

    return level.getHeightAt(position.x, position.z) <= position.y + 1;
}

bool FarmlandBlock::maintainsFarmland(Level &level, const Vector3i &position) {
    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    return VanillaBlocks::getAs<CropBlock>(above.mName) != nullptr;
}

void FarmlandBlock::turnToDirt(Level &level, const Vector3i &position) {
    level.setBlock(position, BlockState("minecraft:dirt"), true);
}
