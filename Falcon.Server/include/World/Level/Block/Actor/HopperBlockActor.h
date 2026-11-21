#pragma once

#include "World/Level/Block/Actor/ContainerBlockActor.h"

class Level;
class ServerNetworkHandler;

class HopperBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Hopper";

    static const int SIZE = 5;
    static const int COOLDOWN_TICKS = 8;

    HopperBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    static bool addOne(Container &container, const ItemStack &item);

    // one item from the container block at `source` into `target`, as a hopper at `puller` takes it
    static bool pullFrom(ServerNetworkHandler &owner, Level &level, const Vector3i &source, Container &target,
                         const Vector3i &puller);

    // the dropped items whose position lies in the box go into `target`, as much of each as fits
    static bool pickUpItems(ServerNetworkHandler &owner, Level &level, float minX, float minY, float minZ, float maxX,
                            float maxY, float maxZ, Container &target);

    bool tick(ServerNetworkHandler &owner) override;

    int mTransferCooldown = COOLDOWN_TICKS;

private:
    bool _pushItems(Level &level, const BlockState &state);

    bool _pullItems(ServerNetworkHandler &owner);

    bool _pickupItemActors(ServerNetworkHandler &owner);
};
