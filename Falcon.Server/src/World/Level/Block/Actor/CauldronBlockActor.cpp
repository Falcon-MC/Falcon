#include "World/Level/Block/Actor/CauldronBlockActor.h"

#include <algorithm>

namespace {
    const char *TAG_ITEMS = "Items";
    const char *TAG_POTION_ID = "PotionId";
    const char *TAG_POTION_TYPE = "PotionType";
    const char *TAG_COLOR = "CustomColor";

    // ItemColor order and the rgb of each dye
    const char *const DYE_ITEMS[] = {
            "minecraft:black_dye",      "minecraft:red_dye",     "minecraft:green_dye",  "minecraft:brown_dye",
            "minecraft:blue_dye",       "minecraft:purple_dye",  "minecraft:cyan_dye",   "minecraft:light_gray_dye",
            "minecraft:gray_dye",       "minecraft:pink_dye",    "minecraft:lime_dye",   "minecraft:yellow_dye",
            "minecraft:light_blue_dye", "minecraft:magenta_dye", "minecraft:orange_dye", "minecraft:white_dye"};
    const uint32_t DYE_COLORS[] = {0x1D1D21, 0xB02E26, 0x5E7C16, 0x835432, 0x3C44AA, 0x8932B8, 0x169C9C, 0x9D9D97,
                                   0x474F52, 0xF38BAA, 0x80C71F, 0xFED83D, 0x3AB3DA, 0xC74EBD, 0xF9801D, 0xF0F0F0};
    // nothing to mix: the color the game falls back to
    const float NO_DYE_COLOR[4] = {0.2065f, 0.3185f, 0.35f, 0.75f};

    float channelOf(uint32_t rgb, int shift) {
        return (float) ((rgb >> shift) & 0xff) / 255.0f;
    }

    uint32_t byteOf(float value) {
        return (uint32_t) (std::min(1.0f, std::max(0.0f, value)) * 255.0f);
    }
}

int32_t CauldronBlockActor::dyeColorOf(const std::string &itemId) {
    for (int32_t index = 0; index < 16; ++index) {
        if (itemId == DYE_ITEMS[index])
            return index;
    }
    return -1;
}

// the item goes in dye slot 2, then the mix replaces CustomColor and empties the dye slots
// the custom color weighs one, a dye its count (always one here)
void CauldronBlockActor::addDye(int32_t dyeColor) {
    float mixed[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int32_t weight = 0;
    if (mHasColor) {
        const uint32_t rgb = (uint32_t) mColor;
        mixed[0] = channelOf(rgb, 16);
        mixed[1] = channelOf(rgb, 8);
        mixed[2] = channelOf(rgb, 0);
        weight = 1;
    }
    if (dyeColor >= 0 && dyeColor < 16) {
        const float red = channelOf(DYE_COLORS[dyeColor], 16);
        const float green = channelOf(DYE_COLORS[dyeColor], 8);
        const float blue = channelOf(DYE_COLORS[dyeColor], 0);
        mixed[0] += red;
        mixed[1] += green;
        mixed[2] += blue;
        weight += 1;
    }

    float color[4] = {NO_DYE_COLOR[0], NO_DYE_COLOR[1], NO_DYE_COLOR[2], NO_DYE_COLOR[3]};
    if (weight > 0) {
        const float scale = 1.0f / (float) weight;
        color[0] = mixed[0] * scale;
        color[1] = mixed[1] * scale;
        color[2] = mixed[2] * scale;
        color[3] = 1.0f;
    }
    mHasColor = true;
    mColor = (int32_t) (byteOf(color[3]) << 24 | byteOf(color[0]) << 16 | byteOf(color[1]) << 8 | byteOf(color[2]));
}

// the float color packed as ARGB, each channel truncated
uint32_t CauldronBlockActor::getEventColor() const {
    if (!mHasColor)
        return byteOf(NO_DYE_COLOR[3]) << 24 | byteOf(NO_DYE_COLOR[0]) << 16 | byteOf(NO_DYE_COLOR[1]) << 8
               | byteOf(NO_DYE_COLOR[2]);
    return 0xFF000000u | ((uint32_t) mColor & 0xFFFFFFu);
}

void CauldronBlockActor::clearContents() {
    mPotionId = -1;
    mHasColor = false;
    mColor = 0;
}

void CauldronBlockActor::_putFields(Tag &data) const {
    data.put(TAG_ITEMS, mItems);
    data.putShort(TAG_POTION_ID, mPotionId);
    data.putShort(TAG_POTION_TYPE, mPotionType);
    if (mHasColor)
        data.putInt(TAG_COLOR, mColor);
}

Tag CauldronBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag CauldronBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void CauldronBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    const Tag *items = data.get(TAG_ITEMS);
    mItems = items != nullptr && items->isList() ? *items : Tag::ofList(Tag::Type::Compound);
    mPotionId = data.getShort(TAG_POTION_ID, -1);
    mPotionType = data.getShort(TAG_POTION_TYPE, -1);
    mHasColor = data.contains(TAG_COLOR);
    mColor = data.getInt(TAG_COLOR, 0);
}
