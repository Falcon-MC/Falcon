#include "Protocol/Packets/UpdateSoftEnumPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateSoftEnumPacket::UpdateSoftEnumPacket() = default;

void UpdateSoftEnumPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mSoftEnum.mName);
    stream.putUnsignedVarInt((uint32_t) mSoftEnum.mValues.size());
    for (const std::string &value: mSoftEnum.mValues) {
        stream.putString(value);
    }

    stream.putByte((unsigned char) mType);
}

void UpdateSoftEnumPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSoftEnum.mName = stream.getString();

    uint32_t count = stream.getUnsignedVarInt();
    mSoftEnum.mValues.clear();
    mSoftEnum.mValues.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mSoftEnum.mValues.push_back(stream.getString());
    }

    mSoftEnum.mIsSoft = true;
    mType = (SoftEnumUpdateType) stream.getByte();
}

void UpdateSoftEnumPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
