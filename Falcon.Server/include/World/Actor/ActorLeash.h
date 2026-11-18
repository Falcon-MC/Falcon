#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class ServerActor;
class ServerNetworkHandler;
class Tag;
class EntityDataMap;

// a lead from a minecraft:leashable actor to its holder: a player, or an actor with minecraft:leashable_to;
// follows the leashed actor's presets
class ActorLeash {
public:
    bool isLeashed() const {
        return mHolderId != 0;
    }

    int64_t getHolderId() const {
        return mHolderId;
    }

    void attach(ServerNetworkHandler &owner, ServerActor &actor, int64_t holderUniqueId);

    void detach(ServerNetworkHandler &owner, ServerActor &actor, bool dropLead);

    void tick(ServerNetworkHandler &owner, ServerActor &actor);

    void appendSpawnMetadata(EntityDataMap &metadata) const;

    void saveNbt(Tag &data) const;

    void loadNbt(const Tag &data);

private:
    enum class SpringType : uint8_t { Bouncy = 0, Dampened = 1, QuadDampened = 2 };

    struct Preset {
        float mSoftDistance;
        float mHardDistance;
        float mMaxDistance;
        SpringType mSpringType;
        float mRotationAdjustment;
    };

    Preset _preset(ServerNetworkHandler &owner, ServerActor &actor, Actor &holder) const;

    void _pull(ServerActor &actor, const Actor &holder, const Preset &preset, float distance) const;

    void _sync(ServerNetworkHandler &owner, ServerActor &actor);

    int64_t mHolderId = 0;
    // a player holder may be offline and keep the lead; an actor holder that is gone drops it
    bool mHeldByActor = false;
    Vector3f mPathTarget;
};
