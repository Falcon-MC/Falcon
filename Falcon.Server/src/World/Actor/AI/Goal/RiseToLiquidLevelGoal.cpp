#include "World/Actor/AI/Goal/RiseToLiquidLevelGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float PROBE_START = 0.5f;
    const float PROBE_STEP = 0.1f;
    const float SURFACE_DROP = 0.75f;
    const float SETTLE_BAND = 0.4f;
    const int32_t FALLING_DEPTH = 8;

    bool liquidAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state != nullptr && LiquidView(*state).isLiquid();
    }
}

RiseToLiquidLevelGoal::RiseToLiquidLevelGoal(float liquidYOffset, float riseDelta, float sinkDelta)
        : mLiquidYOffset(liquidYOffset), mRiseDelta(riseDelta), mSinkDelta(sinkDelta) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump);
}

bool RiseToLiquidLevelGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const LiquidContact contact = LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition());
    return contact.water || contact.lava;
}

// the game walks up in 0.1 steps from half a block over the feet while the centre is liquid, then takes that
// height, not its block, plus the top block's surface; a column that starts dry gives -1
float RiseToLiquidLevelGoal::targetHeight(Level &level, const Vector3f &feet, float liquidYOffset) {
    const int32_t x = (int32_t) std::floor(feet.x);
    const int32_t z = (int32_t) std::floor(feet.z);
    float top = -1.0f;
    for (float probe = feet.y + PROBE_START; liquidAt(level, x, (int32_t) std::floor(probe), z); probe += PROBE_STEP)
        top = probe;

    // a falling liquid counts as one ninth full
    const BlockState *state = level.peekBlockPtr(x, (int32_t) std::floor(top), z);
    const int32_t depth = state == nullptr ? FALLING_DEPTH : LiquidView(*state).getDepth();
    const float height = (float) (depth < FALLING_DEPTH ? depth + 1 : 1);
    return top + (1.0f - height / 9.0f) - SURFACE_DROP - liquidYOffset;
}

void RiseToLiquidLevelGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Vector3f feet = mob.getPosition();
    const float target = targetHeight(owner.getLevelFor(mob), feet, mLiquidYOffset);
    Vector3f motion = mob.getMotion();
    if (std::fabs(feet.y - target) <= SETTLE_BAND)
        motion.y = 0.0f;
    else
        motion.y += feet.y > target ? -mSinkDelta : mRiseDelta;
    mob.setMotion(motion);
}
