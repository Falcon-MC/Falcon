#include "World/Actor/AI/Goal/MoveIndoorsGoal.h"

#include "World/Actor/AI/Goal/HideGoal.h"
#include "World/Actor/AI/Goal/MoveToPoiGoal.h"
#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Level.h"
#include "World/Village/PoiInstance.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <memory>
#include <random>

namespace {
    // the game's own rain test: a level past 0.2
    const float RAIN_LEVEL_THRESHOLD = 0.2f;
    const int32_t OUTDOORS_RANGE = 10;
    const int32_t OUTDOORS_HEIGHT = 7;

    std::mt19937 &indoorsRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

MoveIndoorsGoal::MoveIndoorsGoal(Direction direction, float speed, int32_t timeoutCooldownTicks)
        : mDirection(direction), mSpeed(speed), mTimeoutCooldownTicks(timeoutCooldownTicks) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveIndoorsGoal::_isUnderSky(ServerNetworkHandler &owner, const MobActor &mob) const {
    const Vector3i block = blockOf(mob.getPosition());
    return block.y >= owner.getLevelFor(mob).getHeightAt(block.x, block.z);
}

bool MoveIndoorsGoal::_wantsToMove(ServerNetworkHandler &owner, const MobActor &mob) const {
    if (mDirection == Direction::Outdoors)
        return !_isUnderSky(owner, mob);
    if (mob.getFlags().get(ActorFlag::InLove) || !_isUnderSky(owner, mob))
        return false;

    Level &level = owner.getLevelFor(mob);
    const bool rained = level.getRainLevel() > RAIN_LEVEL_THRESHOLD
                        && PrecipitationSystem::isRainingOn(level, blockOf(mob.getPosition()));
    return rained || HideGoal::isVillageRaided(mob);
}

bool MoveIndoorsGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldown > 0) {
        --mCooldown;
        return false;
    }
    if (!_wantsToMove(owner, mob))
        return false;

    if (mDirection == Direction::Indoors) {
        const std::shared_ptr<PoiInstance> bed = MoveToPoiGoal::findClaimedPoi(mob, PoiType::Bed);
        if (bed == nullptr)
            return false;
        mTarget = Vector3f((float) bed->mPosition.x + 0.5f, (float) bed->mPosition.y, (float) bed->mPosition.z + 0.5f);
        return true;
    }

    Level &level = owner.getLevelFor(mob);
    Vector3i ground;
    if (!RandomGroundPosition::find(level, mob, OUTDOORS_RANGE, OUTDOORS_HEIGHT, nullptr, indoorsRandom(), ground)
        || ground.y < level.getHeightAt(ground.x, ground.z))
        return false;
    mTarget = Vector3f((float) ground.x + 0.5f, (float) ground.y, (float) ground.z + 0.5f);
    return true;
}

bool MoveIndoorsGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _wantsToMove(owner, mob) && !mob.getNavigation().isDone();
}

void MoveIndoorsGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void MoveIndoorsGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    // still where it set off from: wait the timeout before trying again
    if (_wantsToMove(owner, mob))
        mCooldown = mTimeoutCooldownTicks;
    mob.getNavigation().stop(mob);
}
