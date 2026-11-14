#include "World/Actor/AI/Goal/WorkGoal.h"

#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Level.h"
#include "World/Level/LevelSoundEventIds.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"

#include <random>
#include <utility>

namespace {
    // the village hears of the work on the first tick at the site, then every minute of it
    const int32_t WORK_RECORD_INTERVAL = 1200;
    const float RAIN_LEVEL_THRESHOLD = 0.2f;

    std::mt19937 &workRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

WorkGoal::WorkGoal(Settings settings)
        : MoveToPoiGoal(settings.mSpeed, PoiType::JobSite), mSettings(std::move(settings)) {}

bool WorkGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldownLeft > 0 && --mCooldownLeft != 0)
        return false;

    if (!MoveToPoiGoal::canUse(owner, mob))
        return false;

    if (!mSettings.mCanWorkInRain && _isRainingOnSite(owner, mob))
        return false;

    const std::shared_ptr<PoiInstance> poi = findClaimedPoi(mob, PoiType::JobSite);
    mSound = poi == nullptr ? std::string() : poi->mSound;
    return true;
}

bool WorkGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!MoveToPoiGoal::canContinueToUse(owner, mob))
        return false;

    const int32_t left = mSettings.mActiveTicks - mWorkedTicks;
    if (mSettings.mActiveTicks > 0 && left < 0)
        return false;

    if (mSettings.mCanWorkInRain)
        return true;
    if (mSettings.mRainToleranceTicks >= 0 && left <= mSettings.mRainToleranceTicks)
        return true;
    return !_isRainingOnSite(owner, mob);
}

void WorkGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::start(owner, mob);
    mCooldownLeft = 0;
    mSoundTicks = 0;
    mSoundDelay = _nextSoundDelay();
}

void WorkGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::stop(owner, mob);
    mSoundTicks = 0;
    mCooldownLeft = mSettings.mCooldownTicks;
    mSound.clear();
}

void WorkGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Vector3i &site = getTargetBlock();
    mob.getLookControl().setLookAt(Vector3f((float) site.x + 0.5f, (float) site.y + 1.5f, (float) site.z + 0.5f));

    MoveToPoiGoal::tick(owner, mob);
    if (!isReached())
        return;

    if (mSoundTicks > mSoundDelay) {
        mSoundDelay = _nextSoundDelay();
        mSoundTicks = 0;
        if (LevelSoundEventIds::fromName(mSound) != LevelSoundEventIds::UNDEFINED)
            owner.playLevelSound(owner.getLevelFor(mob), mSound,
                                 Vector3f((float) site.x, (float) site.y, (float) site.z));
    }
    ++mSoundTicks;

    if (mWorkedTicks % WORK_RECORD_INTERVAL == 0) {
        const MobDweller &dweller = mob.getDweller();
        if (const std::shared_ptr<Village> village = dweller.isActive() ? dweller.getVillage() : nullptr)
            village->recordWork(mob.getUniqueId(), owner.getCurrentTick());
    }
    ++mWorkedTicks;

    // once for the goal's life: the schedule rebuilds the goal for each shift
    if (!mArrived) {
        mArrived = true;
        EntityEvents::fireTrigger(owner, mob, mSettings.mOnArrival.get());
    }
}

bool WorkGoal::_isRainingOnSite(ServerNetworkHandler &owner, const MobActor &mob) const {
    Level &level = owner.getLevelFor(mob);
    return level.getRainLevel() > RAIN_LEVEL_THRESHOLD && PrecipitationSystem::isRainingOn(level, getTargetBlock());
}

int32_t WorkGoal::_nextSoundDelay() const {
    if (mSettings.mSoundDelayMax <= mSettings.mSoundDelayMin)
        return mSettings.mSoundDelayMin;
    return mSettings.mSoundDelayMin
           + std::uniform_int_distribution<int32_t>(0, mSettings.mSoundDelayMax - mSettings.mSoundDelayMin
                                                               - 1)(workRandom());
}
