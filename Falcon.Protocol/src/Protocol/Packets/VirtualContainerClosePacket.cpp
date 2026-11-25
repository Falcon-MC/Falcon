#include "Protocol/Packets/VirtualContainerClosePacket.h"

#include "Protocol/NetworkPacketHandler.h"

VirtualContainerClosePacket::VirtualContainerClosePacket() = default;

void VirtualContainerClosePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mSessionId);
    stream.putByte((unsigned char) mKind);
    stream.putByte((unsigned char) mCause);
}

void VirtualContainerClosePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSessionId = stream.getUnsignedVarLong();
    mKind = (VirtualContainerCloseKind) stream.getByte();
    mCause = (VirtualContainerCloseCause) stream.getByte();
}

void VirtualContainerClosePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
