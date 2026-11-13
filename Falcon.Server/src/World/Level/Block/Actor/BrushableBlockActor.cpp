#include "World/Level/Block/Actor/BrushableBlockActor.h"

namespace {
    const char *TAG_TYPE = "type";
    const char *TAG_BRUSH_DIRECTION = "brush_direction";
    const char *TAG_BRUSH_COUNT = "brush_count";
    const char *TAG_ITEM = "item";
}

BrushableBlockActor::BrushableBlockActor() {
    setLootTable(EMPTY_LOOT_TABLE, 0);
}

// BrushableBlockActor::save (sub_143308520, 26.60): the item only while one is held
void BrushableBlockActor::_putFields(Tag &data) const {
    data.putByte(TAG_BRUSH_DIRECTION, (int8_t) mBrushDirection);
    data.putInt(TAG_BRUSH_COUNT, mBrushCount);
    data.putString(TAG_TYPE, mType);
    if (mHasItem)
        data.put(TAG_ITEM, mItem);
}

Tag BrushableBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag BrushableBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

// BrushableBlockActor::load (sub_143308270, 26.60): nothing read without a type; a missing field reads as 0
void BrushableBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    if (!data.contains(TAG_TYPE))
        return;

    mBrushDirection = (uint8_t) data.getByte(TAG_BRUSH_DIRECTION, 0);
    mBrushCount = data.getInt(TAG_BRUSH_COUNT, 0);
    mType = data.getString(TAG_TYPE);
    const Tag *item = data.get(TAG_ITEM);
    mHasItem = item != nullptr && item->isCompound();
    if (mHasItem)
        mItem = *item;
}
