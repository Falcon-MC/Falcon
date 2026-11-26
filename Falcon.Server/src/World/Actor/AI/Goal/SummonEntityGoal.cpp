#include "World/Actor/AI/Goal/SummonEntityGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/SpellCasting.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    const float PI = 3.1415927f;
    const float MIN_LINE_LENGTH = 0.0001f;

    bool solidAt(Level &level, int32_t x, int32_t y, int32_t z, bool &known) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        if (state == nullptr) {
            known = false;
            return false;
        }
        return BlockSupport::isSolid(*state);
    }
}

SummonEntityGoal::SummonEntityGoal(std::vector<Choice> choices) : mChoices(std::move(choices)) {
    setRequiredControlFlags(SpellCasting::CASTING_FLAGS);
}

bool SummonEntityGoal::_capped(ServerNetworkHandler &owner, MobActor &mob, const Choice &choice) const {
    for (const Spawn &spawn: choice.mSequence) {
        if (spawn.mSummonCap <= 0)
            continue;
        const float radius = spawn.mSummonCapRadius;
        const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(radius, radius, radius);
        int32_t count = 0;
        owner.forEachActorNear(mob.getPosition(), radius + mob.getSize().mWidth, [&](ServerActor &actor) {
            if (actor.sharesLevelWith(mob) && actor.getTypeId() == spawn.mEntityType
                && area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
                ++count;
        });
        if (count >= spawn.mSummonCap)
            return true;
    }
    return false;
}

bool SummonEntityGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return false;

    const float distance = mob.distanceSquaredTo(*target);
    std::vector<int32_t> candidates;
    int32_t totalWeight = 0;
    for (int32_t index = 0; index < (int32_t) mChoices.size(); ++index) {
        const Choice &choice = mChoices[index];
        if (distance > choice.mMaxRange * choice.mMaxRange || distance < choice.mMinRange * choice.mMinRange)
            continue;
        if (_capped(owner, mob, choice))
            continue;
        if (choice.mFilters != nullptr && !EntityFilter::test(*choice.mFilters, owner, mob, target))
            continue;
        candidates.push_back(index);
        totalWeight = (int32_t) ((float) totalWeight + choice.mWeight);
    }

    mChoice = -1;
    if (candidates.empty() || totalWeight <= 0)
        return false;

    // weights are walked as floats against an integer roll, as the game does
    int32_t roll = std::uniform_int_distribution<int32_t>(0, totalWeight - 1)(mRandom);
    for (const int32_t index: candidates) {
        const float weight = mChoices[index].mWeight;
        if (weight > (float) roll) {
            mChoice = index;
            break;
        }
        roll = (int32_t) ((float) roll - weight);
    }
    return mChoice >= 0 && mCooldownEndTick <= owner.getCurrentTick();
}

bool SummonEntityGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getTarget(owner) == nullptr)
        return false;
    if (mCastTicks > 0)
        return true;
    return mChoice >= 0 && mStep < (int32_t) mChoices[mChoice].mSequence.size();
}

void SummonEntityGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mStep = 0;
    mDelayTicks = -1;
    mSpawned = 0;
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || mChoice < 0)
        return;

    const Choice &choice = mChoices[mChoice];
    mTargetStart = target->getPosition();
    mSelfStart = mob.getPosition();
    Level &level = owner.getLevelFor(mob);
    if (!choice.mStartSoundEvent.empty())
        owner.playLevelSound(level, choice.mStartSoundEvent, mob.getPosition());

    mCastTicks = choice.mCastDuration;
    SpellCasting::sendColor(owner, mob, choice.mParticleColor);
    if (choice.mDoCasting)
        SpellCasting::setCasting(owner, mob, mCastTicks > 0);

    if (!choice.mSequence.empty() && !choice.mSequence.front().mSoundEvent.empty())
        owner.playLevelSound(level, choice.mSequence.front().mSoundEvent, mob.getPosition());
}

void SummonEntityGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setRequiredControlFlags(SpellCasting::CASTING_FLAGS);
    SpellCasting::setCasting(owner, mob, false);
    if (mChoice >= 0)
        mCooldownEndTick = owner.getCurrentTick() + mChoices[mChoice].mCooldown;
}

void SummonEntityGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || mChoice < 0)
        return;

    const Choice &choice = mChoices[mChoice];
    mob.getLookControl().setLookAt(target->getPosition());
    if (mCastTicks-- == 1) {
        setRequiredControlFlags(SpellCasting::AFTER_CAST_FLAGS);
        SpellCasting::setCasting(owner, mob, false);
    }

    if (mStep >= (int32_t) choice.mSequence.size())
        return;

    const Spawn &spawn = choice.mSequence[mStep];
    ++mDelayTicks;
    const Vector3f &self = mob.getPosition();
    const Vector3f &aim = target->getPosition();
    while (mDelayTicks >= spawn.mBaseDelay && mSpawned < spawn.mCount) {
        // heights from where both stand now; the shape's origin from where they stood when the cast began
        const float bottom = std::min(aim.y, self.y);
        const float top = std::max(aim.y, self.y) + 1.0f;
        const Vector3f &origin = spawn.mAroundTarget ? mTargetStart : mSelfStart;
        float x = origin.x;
        float z = origin.z;
        if (spawn.mShape == Shape::Circle) {
            const float angle = std::atan2(aim.z - self.z, aim.x - self.x)
                                + (float) (2 * mSpawned) * PI / (float) spawn.mCount;
            x += std::cos(angle) * spawn.mSize;
            z += std::sin(angle) * spawn.mSize;
        } else {
            const Vector3f &toward = spawn.mAroundTarget ? mSelfStart : mTargetStart;
            const float dx = toward.x - origin.x;
            const float dz = toward.z - origin.z;
            const float length = std::sqrt(dx * dx + dz * dz);
            const float step = spawn.mSize / (float) spawn.mCount * (float) (mSpawned + 1);
            if (length >= MIN_LINE_LENGTH) {
                x += dx / length * step;
                z += dz / length * step;
            }
        }
        _spawnOne(owner, mob, spawn, x, z, bottom, top);
        mDelayTicks -= spawn.mDelayPerSummon;
        ++mSpawned;
    }

    if (mSpawned < spawn.mCount)
        return;
    mSpawned = 0;
    mDelayTicks = 0;
    ++mStep;
    if (!spawn.mSoundEvent.empty())
        owner.playLevelSound(owner.getLevelFor(mob), spawn.mSoundEvent, mob.getPosition());
}

void SummonEntityGoal::_spawnOne(ServerNetworkHandler &owner, MobActor &mob, const Spawn &spawn, float x, float z,
                                 float bottom, float top) {
    Level &level = owner.getLevelFor(mob);
    const int32_t blockX = (int32_t) std::floor(x);
    const int32_t blockZ = (int32_t) std::floor(z);
    const int32_t lowest = (int32_t) std::floor(bottom);
    int32_t blockY = (int32_t) std::floor(top);
    // down from the top: the first open cell standing on a solid block; one below the lowest is still tried
    while (true) {
        bool known = true;
        const bool open = !solidAt(level, blockX, blockY, blockZ, known);
        const bool supported = solidAt(level, blockX, blockY - 1, blockZ, known);
        if (!known)
            return;
        if (open && supported)
            break;
        if (blockY-- < lowest)
            return;
    }

    float offset = 0.0f;
    const AxisAlignedBB cell((float) blockX, (float) blockY, (float) blockZ, (float) blockX + 1.0f,
                             (float) blockY + 1.0f, (float) blockZ + 1.0f);
    for (const AxisAlignedBB &box: level.getCollisionBoxes(cell))
        offset = std::max(offset, box.mMaxY - (float) blockY);

    const Vector3f position((float) blockX, (float) blockY + offset, (float) blockZ);
    owner.spawnActor(level, spawn.mEntityType, position, [&](ServerActor &actor) {
        if (!spawn.mSummonEvent.empty()) {
            if (MobActor *summoned = dynamic_cast<MobActor *>(&actor))
                summoned->setSpawnEvent(spawn.mSummonEvent);
        }
        actor.setOwnerUniqueId(mob.getUniqueId());
        if (spawn.mLifespan > 0)
            actor.setLimitedLife(spawn.mLifespan);
    });
}
