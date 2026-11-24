#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

class FlowerPotBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "FlowerPot";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    // an empty pot has no plant at all
    void setPlant(const BlockState &plant) {
        mPlant = plant;
        mHasPlant = true;
    }

    bool hasPlant() const {
        return mHasPlant && mPlant.mName != "minecraft:air";
    }

    const BlockState &getPlant() const {
        return mPlant;
    }

    // emptied without dropping anything
    void clearPlant() {
        mPlant = BlockState();
        mHasPlant = false;
    }

    // whatever takes the pot away, its plant drops as an item
    void onRemoved() override;

private:
    void _putFields(Tag &data) const;

    BlockState mPlant;
    bool mHasPlant = false;
};
