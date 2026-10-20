#include "World/Level/Block/Actor/ContainerBlockActor.h"

namespace {
    const char *TAG_PRIMARY = "Primary";
    const char *TAG_SECONDARY = "Secondary";
    const char *TAG_PAGE = "page";
    const char *TAG_TOTAL_PAGES = "totalPages";
    const char *TAG_FACING = "facing";
}

Tag ShulkerBoxBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putByte(TAG_FACING, mFacing);
    return data;
}

void ShulkerBoxBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mFacing = data.getByte(TAG_FACING, mFacing);
}

Tag ShulkerBoxBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_FACING, mFacing);
    return data;
}

Tag ContainerBlockActor::saveNbt() const {
    return mInventory.saveNbt();
}

void ContainerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mInventory.loadNbt(data, context);
}

Tag BeaconBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putInt(TAG_PRIMARY, mPrimaryEffect);
    data.putInt(TAG_SECONDARY, mSecondaryEffect);
    return data;
}

void BeaconBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mPrimaryEffect = data.getInt(TAG_PRIMARY);
    mSecondaryEffect = data.getInt(TAG_SECONDARY);
}

Tag LecternBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putInt(TAG_PAGE, mPage);
    data.putInt(TAG_TOTAL_PAGES, mTotalPages);
    return data;
}

void LecternBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mPage = data.getInt(TAG_PAGE);
    mTotalPages = data.getInt(TAG_TOTAL_PAGES);
}
