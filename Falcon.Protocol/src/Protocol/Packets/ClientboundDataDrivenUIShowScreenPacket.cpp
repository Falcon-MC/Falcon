#include "Protocol/Packets/ClientboundDataDrivenUIShowScreenPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundDataDrivenUIShowScreenPacket::ClientboundDataDrivenUIShowScreenPacket() = default;

void ClientboundDataDrivenUIShowScreenPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mScreenId);
    stream.putLInt((uint32_t) mFormId);
    stream.putOptionalPresent(mHasDataInstanceId);
    if (mHasDataInstanceId) {
        stream.putLInt((uint32_t) mDataInstanceId);
    }
}

void ClientboundDataDrivenUIShowScreenPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mScreenId = stream.getString();
    mFormId = (int32_t) stream.getLInt();
    mHasDataInstanceId = stream.getOptionalPresent();
    if (mHasDataInstanceId) {
        mDataInstanceId = (int32_t) stream.getLInt();
    }
}

void ClientboundDataDrivenUIShowScreenPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
