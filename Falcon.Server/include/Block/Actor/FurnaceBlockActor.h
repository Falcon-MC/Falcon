#pragma once

#include "Block/BlockActor.h"
#include "Block/Inventory/FurnaceInventory.h"

class FurnaceBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Furnace";

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Container *getContainer() override { return &mInventory; }

    bool tick(ServerNetworkHandler &owner) override;

    FurnaceInventory &getInventory() { return mInventory; }

    const FurnaceInventory &getInventory() const { return mInventory; }

    FurnaceInventory mInventory;
    FurnaceKind mKind = FurnaceKind::Furnace;
    int mBurnTime = 0;
    int mMaxBurnTime = 0;
    int mCookTime = 0;
};
