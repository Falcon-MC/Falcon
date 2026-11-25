#include "Protocol/Packets/CreativeContentPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

CreativeContentPacket::CreativeContentPacket() = default;

void CreativeContentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mGroups.size());
    for (const CreativeItemGroup &group: mGroups) {
        stream.putByte(group.mCategory);
        stream.putString(group.mName);
        ItemCodec::writeItemInstance(stream, context, group.mIcon);
    }

    stream.putArrayLength((uint32_t) mItems.size());
    for (const CreativeItemData &item: mItems) {
        stream.putUnsignedVarInt((uint32_t) item.mNetId);
        ItemCodec::writeItemInstance(stream, context, item.mItem);
        stream.putUnsignedVarInt((uint32_t) item.mGroupIndex);
    }
}

void CreativeContentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t groupCount = stream.getArrayLength();
    mGroups.reserve(groupCount);
    for (uint32_t i = 0; i < groupCount; i++) {
        CreativeItemGroup group;
        group.mCategory = stream.getByte();
        group.mName = stream.getString();
        group.mIcon = ItemCodec::readItemInstance(stream, context);
        mGroups.push_back(std::move(group));
    }

    uint32_t itemCount = stream.getArrayLength();
    mItems.reserve(itemCount);
    for (uint32_t i = 0; i < itemCount; i++) {
        CreativeItemData item;
        item.mNetId = (int32_t) stream.getUnsignedVarInt();
        item.mItem = ItemCodec::readItemInstance(stream, context);
        item.mGroupIndex = (int32_t) stream.getUnsignedVarInt();
        mItems.push_back(std::move(item));
    }
}

void CreativeContentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
