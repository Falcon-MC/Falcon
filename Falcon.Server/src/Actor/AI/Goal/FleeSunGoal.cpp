#include "Actor/AI/Goal/FleeSunGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockData.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const int64_t DAY_LENGTH = 24000;
    const int64_t DAYTIME_END = 12000;
    const int32_t ATTEMPTS = 10;
    const int32_t HORIZONTAL_RANGE = 10;
    const int32_t VERTICAL_RANGE = 3;

    std::mt19937 &fleeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockData *data = BlockDataTable::find(level.getBlockState(x, y, z).mName.c_str());
        return data != nullptr && data->mSolid;
    }

    bool isExposed(Level &level, int32_t x, int32_t y, int32_t z) {
        return y >= level.getHeightAt(x, z);
    }
}

FleeSunGoal::FleeSunGoal(float speed) : mSpeed(speed) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool FleeSunGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const int64_t time = ((level.getTime() % DAY_LENGTH) + DAY_LENGTH) % DAY_LENGTH;
    if (time >= DAYTIME_END || level.isRaining())
        return false;

    const Vector3f position = mob.getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    if (!isExposed(level, x, y, z))
        return false;

    std::uniform_int_distribution<int32_t> horizontal(-HORIZONTAL_RANGE, HORIZONTAL_RANGE);
    std::uniform_int_distribution<int32_t> vertical(-VERTICAL_RANGE, VERTICAL_RANGE);
    for (int32_t attempt = 0; attempt < ATTEMPTS; ++attempt) {
        const int32_t candidateX = x + horizontal(fleeRandom());
        const int32_t candidateY = y + vertical(fleeRandom());
        const int32_t candidateZ = z + horizontal(fleeRandom());
        if (!level.isChunkResident(candidateX >> 4, candidateZ >> 4)
            || isExposed(level, candidateX, candidateY, candidateZ)
            || isSolid(level, candidateX, candidateY, candidateZ)
            || isSolid(level, candidateX, candidateY + 1, candidateZ)
            || !isSolid(level, candidateX, candidateY - 1, candidateZ))
            continue;

        mShelter = Vector3f((float) candidateX + 0.5f, (float) candidateY, (float) candidateZ + 0.5f);
        return true;
    }
    return false;
}

bool FleeSunGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void FleeSunGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mShelter, mSpeed);
}
