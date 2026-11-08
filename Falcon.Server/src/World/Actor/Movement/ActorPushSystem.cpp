#include "World/Actor/Movement/ActorPushSystem.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <limits>

namespace {
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float PUSH_STRENGTH = 0.2f * 0.32f;
    // wide enough for the widest box (ender dragon) to reach the pushed actor
    const float PUSH_SEARCH_RADIUS = 16.0f;
    const float UNSET = -std::numeric_limits<float>::infinity();

    AxisAlignedBB boxAround(const Vector3f &position, float width, float height) {
        const float half = width * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half, position.x + half, position.y + height,
                             position.z + half);
    }
}

AxisAlignedBB ActorPushSystem::boundingBoxOf(const ServerActor &actor) {
    const ActorSize size = actor.getSize();
    return boxAround(actor.getPosition(), size.mWidth, size.mHeight);
}

AxisAlignedBB ActorPushSystem::boundingBoxOf(const Actor &actor) {
    if (const ServerActor *serverActor = dynamic_cast<const ServerActor *>(&actor))
        return boundingBoxOf(*serverActor);

    return boxAround(actor.getPosition(), PLAYER_WIDTH, PLAYER_HEIGHT);
}

Vector3f ActorPushSystem::computePush(ServerNetworkHandler &owner, ServerActor &actor, const Vector3f &motion) {
    const AxisAlignedBB self = boundingBoxOf(actor).offset(motion.x, motion.y, motion.z);
    const MobActor *mob = dynamic_cast<const MobActor *>(&actor);
    const PushableByEntityPresets *presets = mob == nullptr ? nullptr : &mob->getPushPresets();
    if (presets != nullptr && presets->mPresets.empty())
        presets = nullptr;
    const float reach = presets == nullptr ? 0.0f : presets->mReach;

    float positiveX = UNSET;
    float negativeX = UNSET;
    float positiveZ = UNSET;
    float negativeZ = UNSET;
    bool touched = false;
    Vector3f kick(0.0f, 0.0f, 0.0f);

    // the box or the ball reach first: the presets' filters only run for a pusher in contact
    const auto inContact = [&](const Actor &pusher, const AxisAlignedBB &box, float width) {
        if (box.intersectsWith(self))
            return true;
        if (reach <= 0.0f)
            return false;
        const float dx = pusher.getPosition().x - actor.getPosition().x;
        const float dz = pusher.getPosition().z - actor.getPosition().z;
        const float limit = reach + width * 0.5f;
        return dx * dx + dz * dz <= limit * limit;
    };

    const auto pushBy = [&](const Actor &pusher, const AxisAlignedBB &box, float width, const Vector3f &velocity) {
        const bool overlaps = box.intersectsWith(self);
        const PushableByEntityPreset *preset = presets == nullptr ? nullptr
                                               : PushableByEntity::findPreset(owner, *mob, *presets, pusher);
        // sub_143A0AF90: push_mode none leaves without pushing
        if (preset != nullptr && preset->mMode == PushMode::None)
            return;

        if (preset != nullptr && preset->mMode == PushMode::Ball) {
            const float offsetX = pusher.getPosition().x - actor.getPosition().x;
            const float offsetZ = pusher.getPosition().z - actor.getPosition().z;
            if ((preset->mRequireCollisionOverlap && !overlaps)
                || !PushableByEntity::inReach(*preset, offsetX, offsetZ, width))
                return;

            const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y
                                          + velocity.z * velocity.z);
            const Vector3f added = PushableByEntity::kick(*preset, offsetX, offsetZ, speed, actor.isOnGround());
            kick = Vector3f(kick.x + added.x, kick.y + added.y, kick.z + added.z);
            _playKickSound(owner, actor, *preset, added);
            return;
        }

        if (!overlaps)
            return;

        _accumulate(self, box, positiveX, negativeX, positiveZ, negativeZ);
        touched = true;
    };

    // box test before the virtual checks: getPhysics() on a mob resolves several definition components,
    // keep it out of the pair loop
    owner.forEachActorNear(actor.getPosition(), PUSH_SEARCH_RADIUS, [&](const ServerActor &other) {
        if (&other == &actor || other.isProjectile() || !other.sharesLevelWith(actor))
            return;

        const AxisAlignedBB box = boundingBoxOf(other);
        const float width = other.getSize().mWidth;
        if (!inContact(other, box, width) || !other.isAlive() || !other.isPushable())
            return;

        pushBy(other, box, width, other.getMotion());
    });

    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.sharesLevelWith(actor)
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;

        const AxisAlignedBB box = boxAround(player.getPosition(), PLAYER_WIDTH, PLAYER_HEIGHT);
        if (!inContact(player, box, PLAYER_WIDTH))
            continue;

        pushBy(player, box, PLAYER_WIDTH, player.getMotion());
    }

    if (!touched)
        return kick;

    const float resultX = (positiveX == UNSET ? 0.0f : positiveX) - (negativeX == UNSET ? 0.0f : negativeX);
    const float resultZ = (positiveZ == UNSET ? 0.0f : positiveZ) - (negativeZ == UNSET ? 0.0f : negativeZ);
    const float length = std::sqrt(resultX * resultX + resultZ * resultZ);
    if (!(length > 0.0f))
        return kick;

    return Vector3f(kick.x - resultX / length * PUSH_STRENGTH, kick.y, kick.z - resultZ / length * PUSH_STRENGTH);
}

void ActorPushSystem::_playKickSound(ServerNetworkHandler &owner, ServerActor &actor,
                                     const PushableByEntityPreset &preset, const Vector3f &kick) {
    // sub_143A0AF90: the sound has its own cooldown and needs a strong enough kick
    if (!preset.mPlaySound)
        return;

    const int64_t tick = owner.getCurrentTick();
    const float impulse = kick.x * kick.x + kick.y * kick.y + kick.z * kick.z;
    if (tick - actor.getPushSoundTick() < preset.mSoundCooldownTicks
        || impulse < preset.mSoundImpulseThreshold * preset.mSoundImpulseThreshold)
        return;

    const Vector3f position = actor.getPosition();
    owner.playLevelSound(owner.getLevelFor(actor), PushableByEntity::KICK_SOUND,
                         Vector3f(position.x, position.y + actor.getSize().mHeight * 0.5f, position.z),
                         actor.getIdentifier());
    actor.setPushSoundTick(tick);
}

void ActorPushSystem::_accumulate(const AxisAlignedBB &self, const AxisAlignedBB &other, float &positiveX,
                                  float &negativeX, float &positiveZ, float &negativeZ) {
    const float selfWidthX = self.mMaxX - self.mMinX;
    const float selfWidthZ = self.mMaxZ - self.mMinZ;
    const float centerX = (other.mMaxX + other.mMinX - self.mMaxX - self.mMinX) * 0.5f;
    const float centerZ = (other.mMaxZ + other.mMinZ - self.mMaxZ - self.mMinZ) * 0.5f;

    if (centerX > 0.0f) {
        const float value = (other.mMaxX - other.mMinX) + selfWidthX * 0.5f - centerX;
        if (value > positiveX)
            positiveX = value;
    } else {
        const float value = (other.mMaxX - other.mMinX) + selfWidthX * 0.5f + centerX;
        if (value > negativeX)
            negativeX = value;
    }

    if (centerZ > 0.0f) {
        const float value = (other.mMaxZ - other.mMinZ) + selfWidthZ * 0.5f - centerZ;
        if (value > positiveZ)
            positiveZ = value;
    } else {
        const float value = (other.mMaxZ - other.mMinZ) + selfWidthZ * 0.5f + centerZ;
        if (value > negativeZ)
            negativeZ = value;
    }
}
