#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

class EnderChestBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "EnderChest";

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Container *getContainer() override { return nullptr; }

    Container *getContainerFor(int64_t ownerId);
};
