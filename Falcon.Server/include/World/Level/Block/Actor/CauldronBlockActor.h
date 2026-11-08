#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>

// the potion or dye a cauldron holds; plain water is -1 for both
class CauldronBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Cauldron";

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

private:
    void _putFields(Tag &data) const;

    int16_t mPotionId = -1;
    int16_t mPotionType = -1;
    Tag mItems = Tag::ofList(Tag::Type::Compound);
    bool mHasColor = false;
    int32_t mColor = 0;
};
