#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>
#include <string>

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

    // the dye color index of a dye item (black 0 to white 15), -1 for any other item
    static int32_t dyeColorOf(const std::string &itemId);

    // mixes one item into the water color; a non-dye adds nothing but the color is mixed again all the same
    void addDye(int32_t dyeColor);

    // a generated cauldron: the potion alone, its type stays unset like the game's
    void setPotion(int16_t potionId) {
        mPotionId = potionId;
    }

    bool hasPotion() const {
        return mPotionId >= 0;
    }

    bool hasColor() const {
        return mHasColor;
    }

    // 0xAARRGGBB
    int32_t getColor() const {
        return mColor;
    }

    // the water as the level events show it: the custom color with full alpha, else the game's fallback
    uint32_t getEventColor() const;

    // an emptied cauldron forgets its potion and its color
    void clearContents();

private:
    void _putFields(Tag &data) const;

    int16_t mPotionId = -1;
    int16_t mPotionType = -1;
    Tag mItems = Tag::ofList(Tag::Type::Compound);
    bool mHasColor = false;
    int32_t mColor = 0;
};
