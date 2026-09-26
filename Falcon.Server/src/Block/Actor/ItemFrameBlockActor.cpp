#include "Block/Actor/ItemFrameBlockActor.h"

#include "Inventory/ItemStackNbt.h"

namespace {
    const char *TAG_ITEM = "Item";
    const char *TAG_ITEM_ROTATION = "ItemRotation";
    const char *TAG_ITEM_DROP_CHANCE = "ItemDropChance";
}

void ItemFrameBlockActor::setRotation(int rotation) {
    const int wrapped = ((rotation % ROTATION_COUNT) + ROTATION_COUNT) % ROTATION_COUNT;
    mRotation = (int8_t) wrapped;
}

Tag ItemFrameBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.put(TAG_ITEM, ItemStackNbt::write(mItem));
    data.putByte(TAG_ITEM_ROTATION, mRotation);
    data.putFloat(TAG_ITEM_DROP_CHANCE, mDropChance);
    return data;
}

void ItemFrameBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    const Tag *item = data.get(TAG_ITEM);
    mItem = item == nullptr ? ItemStack::air() : ItemStackNbt::read(*item, context);

    setRotation(data.getByte(TAG_ITEM_ROTATION, 0));
    mDropChance = data.getFloat(TAG_ITEM_DROP_CHANCE, 1.0f);
}

Tag ItemFrameBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    if (isEmpty())
        return data;

    data.put(TAG_ITEM, ItemStackNbt::write(mItem));
    data.putByte(TAG_ITEM_ROTATION, mRotation);
    return data;
}
