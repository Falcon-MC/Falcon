#include "World/Actor/AI/Goal/MoveToWaterGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const BlockData *data = state == nullptr ? nullptr : BlockDataTable::find(state->mName.c_str());
        return data != nullptr && data->mSolid;
    }

    bool holdsWater(const BlockState &state) {
        const LiquidView liquid(state);
        return liquid.isWater() || liquid.isBubbleColumn();
    }

    // a section without water or bubble column in its palettes is skipped without asking each block
    bool sectionMayHoldWater(Level &level, int32_t x, int32_t y, int32_t z) {
        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            return false;

        const LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        return chunk != nullptr && chunk->getSubChunk(LevelChunk::subChunkIndexFor(y))
                .paletteMatches(SubChunk::PaletteQuery::Water, holdsWater);
    }

    // as LiquidPhysicsSystem reads it: layer 0, or layer 1's water when layer 0 can be waterlogged;
    // cheap layers first spare the block data lookup for almost every block
    bool holdsWaterAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *layer0 = level.peekBlockPtr(x, y, z, 0);
        if (layer0 == nullptr)
            return false;

        const LiquidView liquid(*layer0);
        if (liquid.isWater() || liquid.isBubbleColumn())
            return true;
        if (liquid.isLava())
            return false;

        const BlockState *layer1 = level.peekBlockPtr(x, y, z, 1);
        return layer1 != nullptr && LiquidView(*layer1).isWater()
               && LiquidPhysicsSystem::getWaterloggingLevel(*layer0) != 0;
    }

    bool isOpen(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state != nullptr && !isSolid(level, x, y, z)
               && !holdsWaterAt(level, x, y, z) && !holdsWaterAt(level, x, y + 1, z);
    }
}

MoveToWaterGoal::MoveToWaterGoal(float speed, int32_t searchRange, int32_t searchHeight, float goalRadius,
                                 bool towardsWater)
        : mSpeed(speed), mSearchRange(searchRange), mSearchHeight(searchHeight), mGoalRadius(goalRadius),
          mTowardsWater(towardsWater) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveToWaterGoal::_isInWater(ServerNetworkHandler &owner, const MobActor &mob) const {
    return LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water;
}

bool MoveToWaterGoal::_findTarget(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &target) const {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    float nearest = -1.0f;
    for (int32_t x = originX - mSearchRange; x <= originX + mSearchRange; ++x) {
        for (int32_t y = originY - mSearchHeight; y <= originY + mSearchHeight; ++y) {
            for (int32_t z = originZ - mSearchRange; z <= originZ + mSearchRange; ++z) {
                // the water test reads the block and the one above, as a body 1.8 blocks tall standing there
                if (mTowardsWater && !sectionMayHoldWater(level, x, y, z) && !sectionMayHoldWater(level, x, y + 1, z))
                    continue;

                // LiquidBlocksFetch::at(candidate).water, for a body 1.8 blocks tall centred on the block:
                // water in the block or the one above
                const Vector3f candidate((float) x + 0.5f, (float) y, (float) z + 0.5f);
                const bool matches = mTowardsWater
                                     ? holdsWaterAt(level, x, y, z) || holdsWaterAt(level, x, y + 1, z)
                                     : isSolid(level, x, y - 1, z) && isOpen(level, x, y, z) && isOpen(level, x, y + 1, z);
                if (!matches)
                    continue;

                const float dx = candidate.x - position.x;
                const float dy = candidate.y - position.y;
                const float dz = candidate.z - position.z;
                const float distance = dx * dx + dy * dy + dz * dz;
                if (nearest < 0.0f || distance < nearest) {
                    nearest = distance;
                    target = candidate;
                }
            }
        }
    }
    return nearest >= 0.0f;
}

bool MoveToWaterGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _isInWater(owner, mob) != mTowardsWater && _findTarget(owner, mob, mTarget);
}

bool MoveToWaterGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (_isInWater(owner, mob) == mTowardsWater || mob.getNavigation().isDone())
        return false;

    const Vector3f position = mob.getPosition();
    const float dx = mTarget.x - position.x;
    const float dy = mTarget.y - position.y;
    const float dz = mTarget.z - position.z;
    return dx * dx + dy * dy + dz * dz > mGoalRadius * mGoalRadius;
}

void MoveToWaterGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void MoveToWaterGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
