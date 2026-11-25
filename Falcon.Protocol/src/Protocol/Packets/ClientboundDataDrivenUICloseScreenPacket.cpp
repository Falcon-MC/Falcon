#include "Protocol/Packets/ClientboundDataDrivenUICloseScreenPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundDataDrivenUICloseScreenPacket::ClientboundDataDrivenUICloseScreenPacket() = default;

void ClientboundDataDrivenUICloseScreenPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasFormId);
    if (mHasFormId) {
        stream.putLInt((uint32_t) mFormId);
    }
}

void ClientboundDataDrivenUICloseScreenPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasFormId = stream.getOptionalPresent();
    if (mHasFormId) {
        mFormId = (int32_t) stream.getLInt();
    }
}

void ClientboundDataDrivenUICloseScreenPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
