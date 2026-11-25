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

    void setPotion(int16_t potionId, int16_t potionType) {
        mPotionId = potionId;
        mPotionType = potionType;
    }

    void clearPotion() {
        mPotionId = -1;
    }

    bool hasPotion() const {
        return mPotionId >= 0;
    }

    int16_t getPotionId() const {
        return mPotionId;
    }

    // 0 drinkable, 1 splash, 2 lingering; -1 for a generated cauldron
    int16_t getPotionType() const {
        return mPotionType;
    }

    bool hasColor() const {
        return mHasColor;
    }

    // 0xAARRGGBB
    int32_t getColor() const {
        return mColor;
    }

    // the liquid as the level events show it: the potion's color, else the custom color with full alpha, else the
    // game's fallback
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
