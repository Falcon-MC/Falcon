#include "Protocol/Packets/TransferPacket.h"

#include "Protocol/NetworkPacketHandler.h"

TransferPacket::TransferPacket() = default;

void TransferPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mAddress);
    stream.putLShort((uint16_t) mPort);
    stream.putBool(mReloadWorld);

    stream.putOptionalPresent(mHasGatheringsConfigurationJoinInfo);
    if (mHasGatheringsConfigurationJoinInfo)
        mGatheringsConfigurationJoinInfo.write(stream);
}

void TransferPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAddress = stream.getString();
    mPort = stream.getLShort();
    mReloadWorld = stream.getBool();

    mHasGatheringsConfigurationJoinInfo = stream.getOptionalPresent();
    if (mHasGatheringsConfigurationJoinInfo)
        mGatheringsConfigurationJoinInfo.read(stream);
}

void TransferPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
