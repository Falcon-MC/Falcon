#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Actor/Mob/Hostile/HostileActor.h"

#include <cstdint>

class Level;

// stuck to a face of its block, it opens its shell to look and shoot, and teleports when its block fails it
class ShulkerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:shulker";

    // the game's synced data: how far the shell is open, the face it is stuck to, whether its block is set, the
    // block
    static constexpr int32_t PEEK_DATA_ID = 64;
    static constexpr int32_t ATTACH_FACE_DATA_ID = 65;
    static constexpr int32_t ATTACHED_DATA_ID = 66;
    static constexpr int32_t ATTACH_POSITION_DATA_ID = 67;

    static constexpr int32_t PEEK_CLOSED = 0;
    // a look around with no target
    static constexpr int32_t PEEK_IDLE = 30;
    // open on a target
    static constexpr int32_t PEEK_OPEN = 100;

    // the faces in the game's Facing order: down, up, north, south, west, east
    static constexpr int8_t FACE_DOWN = 0;
    static constexpr int8_t FACE_COUNT = 6;

    static constexpr int32_t TELEPORT_TRIES = 5;
    // a teleport lands within this many blocks on each axis
    static constexpr int32_t TELEPORT_RANGE = 8;

    using HostileActor::HostileActor;

    bool shouldRemoveInPeaceful() const override {
        return MobActor::shouldRemoveInPeaceful();
    }

    ActorSize getSize() const override {
        return ActorSize{0.99f, 0.99f};
    }

    float getDefaultMaxHealth() const override {
        return 30.0f;
    }

    bool preventsSleep() const override {
        return false;
    }

    // the game turns a new shulker's body and head to face south
    void finalizeSpawn() override;

    void tick(ServerNetworkHandler &owner) override;

    // stuck to its block instead of moving; a rider moves with its vehicle
    void tickMovement(ServerNetworkHandler &owner) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    // up to five random blocks around; false when none would hold it
    bool teleport(ServerNetworkHandler &owner);

    // a block it could be stuck to, picked like a teleport's target; false when no face of it holds
    bool teleportTo(ServerNetworkHandler &owner, const Vector3i &target);

    int32_t getPeek() const {
        return mPeek;
    }

    int8_t getAttachFace() const {
        return mAttachFace;
    }

    const Vector3i &getAttachPosition() const {
        return mAttachPosition;
    }

    bool isAttached() const {
        return mAttached;
    }

private:
    void _tickPeek(ServerNetworkHandler &owner);

    void _setPeek(ServerNetworkHandler &owner, int32_t peek);

    void _setAttachFace(ServerNetworkHandler &owner, int8_t face);

    void _setAttachPosition(ServerNetworkHandler &owner, const Vector3i &position);

    // the first face, in Facing order, whose neighbour holds it and whose opposite cell is free
    bool _findAttachment(ServerNetworkHandler &owner, const Vector3i &position);

    bool _holds(Level &level, const Vector3i &support, int8_t face) const;

    // the cell its shell opens into: neither a solid block nor another shulker
    bool _isOpenCell(ServerNetworkHandler &owner, const Vector3i &cell) const;

    bool _shulkerAt(ServerNetworkHandler &owner, const Vector3i &cell) const;

    void _duplicate(ServerNetworkHandler &owner, const Vector3f &at);

    // a piston or its arm entered its block: the block moves on
    bool _pushedByPiston(ServerNetworkHandler &owner);

    void _sync(ServerNetworkHandler &owner, int32_t dataId);

    Vector3i mAttachPosition;
    int8_t mAttachFace = FACE_DOWN;
    bool mAttached = false;
    int32_t mPeek = PEEK_CLOSED;
    int32_t mPeekDuration = 0;
    bool mHadTarget = false;
    // the damage being taken comes from a shulker bullet
    bool mHitByBullet = false;
};
