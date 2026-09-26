#include "Actor/AI/Goal/TakeBlockGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>
#include <utility>

namespace {
    const char *const GRIEFING_RULE = "mobgriefing";
    const float EYE_RATIO = 0.85f;
    const float BLOCK_CLEARANCE = 0.9f;
    const double SIGHT_STEP = 0.25;

    std::mt19937 &takeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomBetween(int32_t minimum, int32_t maximum) {
        if (maximum <= minimum)
            return minimum;
        return std::uniform_int_distribution<int32_t>(minimum, maximum)(takeRandom());
    }

    bool canSee(Level &level, const MobActor &mob, const Vector3i &position) {
        const Vector3f feet = mob.getPosition();
        const Vector3f eye(feet.x, feet.y + mob.getSize().mHeight * EYE_RATIO, feet.z);
        const float centerX = (float) position.x + 0.5f;
        const float centerY = (float) position.y + 0.5f;
        const float centerZ = (float) position.z + 0.5f;
        const float dx = eye.x - centerX;
        const float dy = eye.y - centerY;
        const float dz = eye.z - centerZ;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length <= BLOCK_CLEARANCE)
            return true;

        const float scale = BLOCK_CLEARANCE / length;
        return !Explosion::isRayCollidingWithBlocks(level, eye.x, eye.y, eye.z, centerX + dx * scale,
                                                    centerY + dy * scale, centerZ + dz * scale, SIGHT_STEP);
    }
}

TakeBlockGoal::TakeBlockGoal(Settings settings) : mSettings(std::move(settings)) {
}

bool TakeBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.hasCarriedBlock() || std::uniform_real_distribution<float>(0.0f, 1.0f)(takeRandom()) >= mSettings.mChance)
        return false;

    Level &level = owner.getLevelFor(mob);
    if (!level.getGameRules().getBool(GRIEFING_RULE))
        return false;

    const Vector3f feet = mob.getPosition();
    const Vector3i position((int32_t) std::floor(feet.x) + randomBetween(mSettings.mMinXz, mSettings.mMaxXz),
                            (int32_t) std::floor(feet.y) + randomBetween(mSettings.mMinY, mSettings.mMaxY),
                            (int32_t) std::floor(feet.z) + randomBetween(mSettings.mMinXz, mSettings.mMaxXz));
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr || mSettings.mBlocks.count(state->mName) == 0 || !canSee(level, mob, position))
        return false;

    mPosition = position;
    return true;
}

bool TakeBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void TakeBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const BlockState taken = level.getBlockState(mPosition.x, mPosition.y, mPosition.z);
    level.setBlock(mPosition, BlockState("minecraft:air"), true);
    mob.setCarriedBlock(owner, taken);
}
