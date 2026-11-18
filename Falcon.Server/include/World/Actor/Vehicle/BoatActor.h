#pragma once

#include "World/Actor/ActorLeash.h"
#include "World/Actor/Movement/PushableByEntity.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Vehicle/VehicleChest.h"

#include <cstdint>
#include <memory>
#include <string>

class Level;
class PlayerAuthInputPacket;

// minecraft:boat and minecraft:chest_boat; the bamboo raft is the boat of variant 7
class BoatActor final : public ServerActor {
public:
    static constexpr const char *BOAT = "minecraft:boat";
    static constexpr const char *CHEST_BOAT = "minecraft:chest_boat";
    static constexpr int32_t RAFT_VARIANT = 7;
    // minecraft:collision_box of both boat definitions
    static constexpr float WIDTH = 1.4f;
    static constexpr float HEIGHT = 0.455f;

    BoatActor(uint64_t runtimeId, const std::string &identifier);

    static bool matches(const std::string &identifier) {
        return identifier == BOAT || identifier == CHEST_BOAT;
    }

    // the variant of a boat or chest boat item, -1 for another item
    static int32_t variantOfItem(const std::string &item);

    // the boat or chest boat item of a variant
    static std::string itemOf(int32_t variant, bool chest);

    // the rider in control predicts the boat: its seat gives the boat's position and its input the boat's turn
    static void followController(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &feetPosition,
                                 const PlayerAuthInputPacket &packet);

    int32_t getVariant() const {
        return mVariant;
    }

    void setVariant(int32_t variant) {
        mVariant = variant;
    }

    bool isChestBoat() const {
        return mChest != nullptr;
    }

    ActorSize getSize() const override;

    Vector3f getSeatOffset(size_t index, size_t passengerCount) const override;

    Container *getEntityContainer() override;

    const json::Value *getLeashableComponent() const override;

    ActorLeash *getActorLeash() override {
        return &mLeash;
    }

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

private:
    // the component groups the definition swaps in and out
    enum class Water : uint8_t { Floating, ColumnUp, ColumnDown, Sunk };

    ServerPlayer *_controller(ServerNetworkHandler &owner) const;

    size_t _seatCount() const;

    bool _canSeat(float riderWidth) const;

    void _damage(ServerNetworkHandler &owner, float amount, ServerPlayer *source);

    void _destroy(ServerNetworkHandler &owner, bool drop);

    void _sendHurt(ServerNetworkHandler &owner);

    void _tickWater(ServerNetworkHandler &owner);

    void _pullInMobs(ServerNetworkHandler &owner);

    // leashable_to: the leads the player holds move to the boat
    bool _takeLeads(ServerNetworkHandler &owner, ServerPlayer &player);

    bool _useLead(ServerNetworkHandler &owner, ServerPlayer &player);

    // the legacy_boat push of the pushers touching the boat; the pushed actors take their share at once
    Vector3f _push(ServerNetworkHandler &owner);

    float _momentum(Level &level) const;

    int32_t mVariant = 0;
    std::unique_ptr<VehicleChest> mChest;
    ActorLeash mLeash;
    PushableByEntityPresets mPushPresets;
    Water mWater = Water::Floating;
    int64_t mSinkTick = -1;
    int32_t mHurtTime = 0;
    int32_t mHurtDirection = 1;
    float mDamage = 0.0f;
    bool mExpired = false;
};
