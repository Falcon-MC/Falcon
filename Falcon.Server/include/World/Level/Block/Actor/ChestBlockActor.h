#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Inventory/ChestInventory.h"

#include <memory>

class ChestBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Chest";

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool isPaired() const { return mPaired; }

    const Vector3i &getPairPosition() const { return mPairPosition; }

    void pairWith(ChestBlockActor &other);

    void unpair();

    ChestBlockActor *getPair();

    ChestBlockActor *getPrimary();

    Container *getContainer() override;

    void addViewer();

    void removeViewer();

    int getViewerCount() const;

    ChestInventory &getInventory() { return mInventory; }

    const ChestInventory &getInventory() const { return mInventory; }

private:
    ChestInventory mInventory;
    std::unique_ptr<DoubleChestInventory> mDoubleInventory;
    int mViewers = 0;
    bool mPaired = false;
    bool mPairLead = false;
    Vector3i mPairPosition;
};
