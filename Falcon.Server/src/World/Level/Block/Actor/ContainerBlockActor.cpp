#include "World/Level/Block/Actor/ContainerBlockActor.h"

#include "World/Inventory/ItemStackNbt.h"

namespace {
    const char *TAG_PRIMARY = "Primary";
    const char *TAG_SECONDARY = "Secondary";
    const char *TAG_PAGE = "page";
    const char *TAG_TOTAL_PAGES = "totalPages";
    const char *TAG_FACING = "facing";
    const char *TAG_RECORD_ITEM = "RecordItem";
    const char *TAG_FINISHED_RECORDING = "FinishedRecording";
    const char *TAG_TICKS_PLAYING = "TicksPlaying";
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

// the record alone goes to disk; TicksPlaying and FinishedRecording ride the update packet only (sub_144C358E0)
Tag JukeboxBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    const ItemStack &record = mInventory.getContainerItem(0);
    if (!record.isAir() && record.mCount > 0)
        data.put(TAG_RECORD_ITEM, ItemStackNbt::write(record));
    return data;
}

void JukeboxBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mFinishedRecording = true;
    mTicksPlaying = 0;
    mInventory.clear();
    if (const Tag *record = data.get(TAG_RECORD_ITEM))
        mInventory.setContainerItem(0, ItemStackNbt::read(*record, context));
}

// the client reads both back from the update packet to start or stop the disc (sub_144C366D0)
Tag JukeboxBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_FINISHED_RECORDING, mFinishedRecording ? 1 : 0);
    data.putInt(TAG_TICKS_PLAYING, mTicksPlaying);
    const ItemStack &record = mInventory.getContainerItem(0);
    if (!record.isAir() && record.mCount > 0)
        data.put(TAG_RECORD_ITEM, ItemStackNbt::write(record));
    return data;
}
