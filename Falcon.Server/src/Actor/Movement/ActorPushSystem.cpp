#include "Actor/Movement/ActorPushSystem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <limits>

namespace {
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float PUSH_STRENGTH = 0.2f * 0.32f;
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

Vector3f ActorPushSystem::computePush(ServerNetworkHandler &owner, const ServerActor &actor, const Vector3f &motion) {
    const AxisAlignedBB self = boundingBoxOf(actor).offset(motion.x, motion.y, motion.z);

    float positiveX = UNSET;
    float negativeX = UNSET;
    float positiveZ = UNSET;
    float negativeZ = UNSET;
    bool touched = false;

    for (auto &entry: owner.getActors()) {
        const ServerActor &other = *entry.second;
        if (&other == &actor || !other.isAlive() || other.isProjectile() || !other.getPhysics().mPushable
            || other.getDimension() != actor.getDimension())
            continue;

        const AxisAlignedBB box = boundingBoxOf(other);
        if (!box.intersectsWith(self))
            continue;

        _accumulate(self, box, positiveX, negativeX, positiveZ, negativeZ);
        touched = true;
    }

    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != actor.getDimension()
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;

        const AxisAlignedBB box = boxAround(player.getPosition(), PLAYER_WIDTH, PLAYER_HEIGHT);
        if (!box.intersectsWith(self))
            continue;

        _accumulate(self, box, positiveX, negativeX, positiveZ, negativeZ);
        touched = true;
    }

    if (!touched)
        return Vector3f(0.0f, 0.0f, 0.0f);

    const float resultX = (positiveX == UNSET ? 0.0f : positiveX) - (negativeX == UNSET ? 0.0f : negativeX);
    const float resultZ = (positiveZ == UNSET ? 0.0f : positiveZ) - (negativeZ == UNSET ? 0.0f : negativeZ);
    const float length = std::sqrt(resultX * resultX + resultZ * resultZ);
    if (!(length > 0.0f))
        return Vector3f(0.0f, 0.0f, 0.0f);

    return Vector3f(-(resultX / length * PUSH_STRENGTH), 0.0f, -(resultZ / length * PUSH_STRENGTH));
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
