#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

// the block actor of a pack block declaring minecraft:block_entity (block actor type 62, sub_143301110)
class DataDrivenBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "DataDriven";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // gone with its block: a pack block has no class to remove it when broken or replaced
    bool tick(ServerNetworkHandler &owner) override;
};
