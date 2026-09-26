#include "Actor/AI/Goal/SquidMovementGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockData.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

#include <cmath>
#include <random>

namespace {
    const char *const FLEE_BEHAVIOR = "minecraft:behavior.squid_flee";
    const char *const DIVE_BEHAVIOR = "minecraft:behavior.squid_dive";
    const char *const GROUND_BEHAVIOR = "minecraft:behavior.squid_move_away_from_ground";
    const char *const OUT_OF_WATER_BEHAVIOR = "minecraft:behavior.squid_out_of_water";
    const EntityEventType SQUID_FLEEING = (EntityEventType) 15;
    const int32_t DIRECTION_CHANGE_CHANCE = 50;
    const float HORIZONTAL_SPEED = 0.2f;
    const float VERTICAL_BASE = -0.1f;
    const float VERTICAL_RANGE = 0.2f;
    const float VERTICAL_ADJUST = 0.1f;
    const float FLEE_SPEED = 0.15f;
    const int32_t FLEE_TICKS = 20;
    const float OUT_OF_WATER_HOP = 0.4f;
    const float OUT_OF_WATER_SPREAD = 0.05f;
    const float TWO_PI = 6.2831855f;

    std::mt19937 &squidRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float unit() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(squidRandom());
    }

    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const BlockData *data = state == nullptr ? nullptr : BlockDataTable::find(state->mName.c_str());
        return data != nullptr && data->mSolid;
    }
}

SquidMovementGoal::SquidMovementGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool SquidMovementGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return true;
}

void SquidMovementGoal::_pickDirection() {
    const float angle = unit() * TWO_PI;
    mDirection = Vector3f(std::cos(angle) * HORIZONTAL_SPEED, VERTICAL_BASE + unit() * VERTICAL_RANGE,
                          std::sin(angle) * HORIZONTAL_SPEED);
    mHasDirection = true;
}

bool SquidMovementGoal::_tickFlee(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getComponent(FLEE_BEHAVIOR) == nullptr)
        return false;

    if (mob.getHurtCount() != mFleeHurtCount) {
        mFleeHurtCount = mob.getHurtCount();
        const Actor *attacker = MobActor::findActor(owner, mob.getLastHurtBy());
        if (attacker != nullptr) {
            const Vector3f position = mob.getPosition();
            const Vector3f from = attacker->getPosition();
            const float dx = position.x - from.x;
            const float dy = position.y - from.y;
            const float dz = position.z - from.z;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length > 0.0f) {
                mDirection = Vector3f(dx / length * FLEE_SPEED, dy / length * FLEE_SPEED, dz / length * FLEE_SPEED);
                mHasDirection = true;
                mFleeTicks = FLEE_TICKS;
                owner.broadcastActorEvent(mob, SQUID_FLEEING);
            }
        }
    }

    if (mFleeTicks <= 0)
        return false;

    --mFleeTicks;
    return true;
}

void SquidMovementGoal::_tickOutOfWater(MobActor &mob) {
    if (mob.getComponent(OUT_OF_WATER_BEHAVIOR) == nullptr || !mob.isOnGround())
        return;

    std::uniform_real_distribution<float> spread(-OUT_OF_WATER_SPREAD, OUT_OF_WATER_SPREAD);
    const Vector3f motion = mob.getMotion();
    mob.setMotion(Vector3f(motion.x + spread(squidRandom()), OUT_OF_WATER_HOP, motion.z + spread(squidRandom())));
    mob.setOnGround(false);
}

void SquidMovementGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    if (!LiquidBlocksFetch::at(level, position).water) {
        mHasDirection = false;
        _tickOutOfWater(mob);
        return;
    }

    if (!_tickFlee(owner, mob)) {
        if (!mHasDirection || std::uniform_int_distribution<int32_t>(0, DIRECTION_CHANGE_CHANCE - 1)(squidRandom()) == 0)
            _pickDirection();

        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t y = (int32_t) std::floor(position.y);
        const int32_t z = (int32_t) std::floor(position.z);
        const float height = mob.getSize().mHeight;
        const Vector3f above(position.x, position.y + height + 1.0f, position.z);
        if (mob.getComponent(DIVE_BEHAVIOR) != nullptr && !LiquidBlocksFetch::at(level, above).water)
            mDirection.y = -VERTICAL_ADJUST;
        else if (mob.getComponent(GROUND_BEHAVIOR) != nullptr && isSolid(level, x, y - 1, z))
            mDirection.y = VERTICAL_ADJUST;
    }

    mob.setMotion(mDirection);
    mob.getNavigation().stop(mob);
}
