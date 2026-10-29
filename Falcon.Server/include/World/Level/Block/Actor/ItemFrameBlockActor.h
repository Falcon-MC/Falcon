#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "Protocol/Types/ItemStack.h"

class ItemFrameBlockActor : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "ItemFrame";

    static const int ROTATION_COUNT = 8;

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    const ItemStack &getItem() const { return mItem; }

    void setItem(ItemStack item) { mItem = std::move(item); }

    bool isEmpty() const { return mItem.isAir() || mItem.mCount <= 0; }

    int getRotation() const { return mRotation; }

    void setRotation(int rotation);

    float getDropChance() const { return mDropChance; }

    void setDropChance(float chance) { mDropChance = chance; }

    int getAnalogOutput() const { return isEmpty() ? 0 : mRotation + 1; }

private:
    ItemStack mItem;
    int8_t mRotation = 0;
    float mDropChance = 1.0f;
};

class GlowItemFrameBlockActor final : public ItemFrameBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "GlowItemFrame";

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};
