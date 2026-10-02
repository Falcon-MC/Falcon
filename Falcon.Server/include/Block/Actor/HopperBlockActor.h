#pragma once

#include "Block/Actor/ContainerBlockActor.h"

class Level;
class ServerNetworkHandler;

class HopperBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Hopper";

    static const int SIZE = 5;
    static const int COOLDOWN_TICKS = 8;

    HopperBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    static bool addOne(Container &container, const ItemStack &item);

    bool tick(ServerNetworkHandler &owner) override;

    int mTransferCooldown = COOLDOWN_TICKS;

private:
    bool _pushItems(Level &level, const BlockState &state);

    bool _pullItems(ServerNetworkHandler &owner);

    bool _pickupItemActors(ServerNetworkHandler &owner);
};
