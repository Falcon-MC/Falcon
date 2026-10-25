#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class MobActor;
class ServerNetworkHandler;
class Tag;
class EntityDataMap;

/** Leash held by a player, following the `minecraft:leashable` presets. */
class MobLeash {
public:
    bool isLeashed() const {
        return mHolderId != 0;
    }

    int64_t getHolderId() const {
        return mHolderId;
    }

    void attach(ServerNetworkHandler &owner, MobActor &mob, int64_t holderUniqueId);

    void detach(ServerNetworkHandler &owner, MobActor &mob, bool dropLead);

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void appendSpawnMetadata(EntityDataMap &metadata) const;

    void saveNbt(Tag &data) const;

    void loadNbt(const Tag &data);

private:
    enum class SpringType : uint8_t {
        Bouncy = 0,
        Dampened = 1,
        QuadDampened = 2
    };

    struct Preset {
        float mSoftDistance;
        float mHardDistance;
        float mMaxDistance;
        SpringType mSpringType;
        float mRotationAdjustment;
    };

    Preset _preset(ServerNetworkHandler &owner, MobActor &mob, Actor &holder) const;

    void _pull(MobActor &mob, const Actor &holder, const Preset &preset, float distance) const;

    void _sync(ServerNetworkHandler &owner, MobActor &mob);

    int64_t mHolderId = 0;
    Vector3f mPathTarget;
};
