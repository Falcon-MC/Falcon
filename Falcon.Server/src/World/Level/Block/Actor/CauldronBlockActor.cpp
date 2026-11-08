#include "World/Level/Block/Actor/CauldronBlockActor.h"

namespace {
    const char *TAG_ITEMS = "Items";
    const char *TAG_POTION_ID = "PotionId";
    const char *TAG_POTION_TYPE = "PotionType";
    const char *TAG_COLOR = "CustomColor";
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
