#include "Protocol/Packets/ClientboundTextureShiftPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundTextureShiftPacket::ClientboundTextureShiftPacket() = default;

void ClientboundTextureShiftPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);
    stream.putString(mCollectionName);
    stream.putString(mFromStep);
    stream.putString(mToStep);

    stream.putArrayLength((uint32_t) mAllSteps.size());
    for (const std::string &step: mAllSteps) {
        stream.putString(step);
    }

    stream.putUnsignedVarLong((uint64_t) mCurrentLengthTicks);
    stream.putUnsignedVarLong((uint64_t) mTotalLengthTicks);
    stream.putBool(mEnabled);
}

void ClientboundTextureShiftPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (Action) stream.getByte();
    mCollectionName = stream.getString();
    mFromStep = stream.getString();
    mToStep = stream.getString();

    uint32_t length = stream.getArrayLength();
    mAllSteps.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        mAllSteps.push_back(stream.getString());
    }

    mCurrentLengthTicks = (int64_t) stream.getUnsignedVarLong();
    mTotalLengthTicks = (int64_t) stream.getUnsignedVarLong();
    mEnabled = stream.getBool();
}

void ClientboundTextureShiftPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
