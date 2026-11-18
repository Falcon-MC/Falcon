#pragma once

#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Vehicle/VehicleChest.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class BlockState;
class Level;
class PlayerAuthInputPacket;

// every minecart: rideable, chest, hopper, TNT and command block
class MinecartActor final : public ServerActor {
public:
    enum class Kind : uint8_t { Rideable, Chest, Hopper, Tnt, CommandBlock };

    // minecraft:collision_box of every minecart definition
    static constexpr float WIDTH = 0.98f;
    static constexpr float HEIGHT = 0.7f;

    MinecartActor(uint64_t runtimeId, const std::string &identifier);

    static bool matches(const std::string &identifier);

    // the minecart actor an item places, empty for another item
    static std::string actorOfItem(const std::string &item);

    Kind getKind() const {
        return mKind;
    }

    bool isMinecart() const override {
        return true;
    }

    ActorSize getSize() const override {
        return ActorSize{WIDTH, HEIGHT};
    }

    Vector3f getSeatOffset(size_t index, size_t passengerCount) const override;

    Container *getEntityContainer() override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    bool onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) override;

    bool isExpired() const override {
        return mExpired;
    }

    void tick(ServerNetworkHandler &owner) override;

    void tickMovement(ServerNetworkHandler &owner) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    void loadEquipment(const Tag &data, const PacketCodecContext &context) override;

    // a command block minecart's command, as the client edits it
    void setCommand(const std::string &command, const std::string &name, bool trackOutput);

    // the rider's forward input, which nudges a stopped minecart
    static void readRider(ServerNetworkHandler &owner, ServerPlayer &player, const PlayerAuthInputPacket &packet);

    // the rail position a point stands on, the way a minecart follows it; nothing off a rail
    static std::optional<Vector3f> railPosition(Level &level, const Vector3f &position);

private:
    void _moveAlongRail(ServerNetworkHandler &owner, Level &level, const Vector3i &rail, const BlockState &state);

    void _moveOffRail(Level &level);

    void _slowDown();

    void _move(Level &level, const Vector3f &motion);

    Vector3f _push(ServerNetworkHandler &owner);

    void _activate(ServerNetworkHandler &owner, bool powered);

    void _pullInMobs(ServerNetworkHandler &owner);

    void _tickTnt(ServerNetworkHandler &owner);

    void _tickHopper(ServerNetworkHandler &owner);

    void _runCommand(ServerNetworkHandler &owner);

    void _prime(ServerNetworkHandler &owner, int32_t fuse);

    void _explode(ServerNetworkHandler &owner);

    void _destroy(ServerNetworkHandler &owner, bool drop);

    void _sendHurt(ServerNetworkHandler &owner);

    ServerPlayer *_rider(ServerNetworkHandler &owner) const;

    Kind mKind;
    std::unique_ptr<VehicleChest> mChest;
    PushableByEntityPresets mPushPresets;
    int32_t mHurtTime = 0;
    int32_t mHurtDirection = 1;
    int32_t mIntegrity = 0;
    bool mExpired = false;
    // the rail_sensor: whether the activator rail under it was powered last tick
    bool mActivated = false;
    // the hopper's item_hopper, switched off by a powered activator rail
    bool mHopperEnabled = true;
    int32_t mHopperCooldown = 0;
    // the primed TNT's ticks left, -1 while not primed
    int32_t mFuse = -1;
    std::string mCommand;
    std::string mCommandName;
    std::string mLastOutput;
    bool mTrackOutput = true;
    int64_t mLastCommandTick = -1;
    float mRiderForward = 0.0f;
};
