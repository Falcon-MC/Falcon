#include "Actor/Misc/ExperienceOrbActor.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

FALCON_REGISTER_ACTOR(ExperienceOrbActor, "minecraft:xp_orb");

namespace {
    const float PLAYER_HEIGHT = 1.8f;
    const float PLAYER_EYE_HEIGHT = 1.62f;
    const int32_t MAX_AGE = 6000;
    const float ATTRACT_RANGE_SQUARED = 64.0f;
    const float ATTRACT_DIVISOR = 8.0f;
    const float ATTRACT_STRENGTH = 0.1f;
    const float GRAVITY = 0.04f;
    const float DRAG = 0.02f;
    const float GROUND_FRICTION = 0.6f;
    const float GROUND_BOUNCE = -0.5f;
    const float PICKUP_REACH = 1.0f;
    const float PICKUP_VOLUME = 0.1f;
    const float PICKUP_PITCH = 1.0f;
}

void ExperienceOrbActor::tick(ServerNetworkHandler &owner) {
    decrementPickupDelay();

    if (getLifetimeTicks() > MAX_AGE) {
        mExpired = true;
        return;
    }

    Vector3f motion = getMotion();
    const Vector3f position = getPosition();

    ServerPlayer *closest = nullptr;
    float closestDistanceSquared = ATTRACT_RANGE_SQUARED;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead())
            continue;
        if (player.getGameType() == (int32_t) GameType::Spectator)
            continue;
        if (player.getDimension() != getDimension())
            continue;

        const Vector3f playerPosition = player.getPosition();
        const float dx = playerPosition.x - position.x;
        const float dy = playerPosition.y - position.y;
        const float dz = playerPosition.z - position.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared < closestDistanceSquared) {
            closest = &player;
            closestDistanceSquared = distanceSquared;
        }
    }

    if (closest != nullptr && getPickupDelay() <= 0 && _tryPickup(owner, *closest)) {
        mExpired = true;
        return;
    }

    if (closest != nullptr)
        _attract(*closest, motion);

    motion.y -= GRAVITY;

    Vector3f next(position.x + motion.x, position.y + motion.y, position.z + motion.z);

    const int32_t blockX = (int32_t) std::floor(next.x);
    const int32_t blockZ = (int32_t) std::floor(next.z);
    const int32_t blockY = (int32_t) std::floor(next.y);

    bool onGround = false;
    if (motion.y < 0.0f && owner.getLevelFor(*this).isSolidAt(blockX, blockY, blockZ)) {
        next.y = (float) (blockY + 1);
        onGround = true;
    }

    float friction = 1.0f - DRAG;
    if (onGround)
        friction *= GROUND_FRICTION;

    motion.x *= friction;
    motion.y *= 1.0f - DRAG;
    motion.z *= friction;

    if (onGround)
        motion.y *= GROUND_BOUNCE;

    setMotion(motion);
    setPosition(next);
    setOnGround(onGround);

    owner.broadcastActorMove(*this);
}

bool ExperienceOrbActor::_tryPickup(ServerNetworkHandler &owner, ServerPlayer &player) {
    const Vector3f position = getPosition();
    const Vector3f playerPosition = player.getPosition();
    const float dx = std::fabs(playerPosition.x - position.x);
    const float dz = std::fabs(playerPosition.z - position.z);

    if (dx > PICKUP_REACH || dz > PICKUP_REACH || position.y < playerPosition.y - PICKUP_REACH
        || position.y > playerPosition.y + PLAYER_HEIGHT)
        return false;

    player.getExperience().addXp(owner.repairWithMending(player, getExperienceValue()));
    player.syncExperience();
    owner.syncPlayerAttributes(player);
    owner.playNamedSound(owner.getLevelFor(*this), "random.orb", position, PICKUP_VOLUME, PICKUP_PITCH);
    return true;
}

void ExperienceOrbActor::_attract(const ServerPlayer &player, Vector3f &motion) const {
    const Vector3f position = getPosition();
    const Vector3f playerPosition = player.getPosition();
    const float dX = (playerPosition.x - position.x) / ATTRACT_DIVISOR;
    const float dY = (playerPosition.y + PLAYER_EYE_HEIGHT * 0.5f - position.y) / ATTRACT_DIVISOR;
    const float dZ = (playerPosition.z - position.z) / ATTRACT_DIVISOR;
    const float distance = std::sqrt(dX * dX + dY * dY + dZ * dZ);
    float diff = 1.0f - distance;

    if (diff <= 0.0f || distance <= 0.0f)
        return;

    diff = diff * diff;
    motion.x += dX / distance * diff * ATTRACT_STRENGTH;
    motion.y += dY / distance * diff * ATTRACT_STRENGTH;
    motion.z += dZ / distance * diff * ATTRACT_STRENGTH;
}
