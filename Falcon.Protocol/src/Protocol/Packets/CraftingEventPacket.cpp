#include "Protocol/Packets/CraftingEventPacket.h"

#include "Protocol/ItemCodec.h"
#include "Protocol/NetworkPacketHandler.h"

CraftingEventPacket::CraftingEventPacket() = default;

void CraftingEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mContainerId);
    stream.putVarInt(craftingTypeToId(mType));
    stream.putUuid(mUuid);

    stream.putArrayLength((uint32_t) mInputs.size());
    for (const ItemStack &item: mInputs) {
        ItemCodec::writeItemInstance(stream, context, item);
    }

    stream.putArrayLength((uint32_t) mOutputs.size());
    for (const ItemStack &item: mOutputs) {
        ItemCodec::writeItemInstance(stream, context, item);
    }
}

void CraftingEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = (int8_t) stream.getByte();
    mType = craftingTypeFromId(stream.getVarInt());
    mUuid = stream.getUuid();

    mInputs.clear();
    uint32_t inputLength = stream.getArrayLength();
    mInputs.reserve(inputLength);
    for (uint32_t i = 0; i < inputLength; i++) {
        mInputs.push_back(ItemCodec::readItemInstance(stream, context));
    }

    mOutputs.clear();
    uint32_t outputLength = stream.getArrayLength();
    mOutputs.reserve(outputLength);
    for (uint32_t i = 0; i < outputLength; i++) {
        mOutputs.push_back(ItemCodec::readItemInstance(stream, context));
    }
}

void CraftingEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
