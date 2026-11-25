#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class Actor;
class Level;
class ServerPlayer;

enum class TeleportCause : uint8_t {
    Command,
    ChorusFruit,
    EnderPearl,
    EndGateway,
    FollowOwner,
    Plugin,
};

// a player or another entity about to teleport; cancelling keeps it where it is
struct ActorTeleportBeforeEvent : public CancellableEvent {
    Actor &mActor;
    Vector3f &mTo;
    TeleportCause mCause;
    // only read back by a teleport that can change level
    Level *mDestination;

    ActorTeleportBeforeEvent(Actor &actor, Vector3f &to, TeleportCause cause, Level *destination = nullptr)
            : mActor(actor), mTo(to), mCause(cause), mDestination(destination) {}
};

// a player about to fill or empty a bucket, before the world changes; cancelling leaves the block as it was
struct PlayerBucketUseBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;
    bool mFill;
    const ItemStack &mBucket;

    PlayerBucketUseBeforeEvent(ServerPlayer &player, Level &level, const Vector3i &position, bool fill,
                               const ItemStack &bucket)
            : mPlayer(player), mLevel(level), mBlockPosition(position), mFill(fill), mBucket(bucket) {}
};

struct TeleportBeforeSignals {
    EventSignal<ActorTeleportBeforeEvent> mActorTeleport;
    EventSignal<PlayerBucketUseBeforeEvent> mPlayerBucketUse;

    void clearTeleportBeforeSignals() {
        mActorTeleport.clear();
        mPlayerBucketUse.clear();
    }
};

struct TeleportAfterSignals {

    void clearTeleportAfterSignals() {}
};
