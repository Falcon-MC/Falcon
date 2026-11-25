#include "Protocol/Packets/OpenSignPacket.h"

#include "Protocol/NetworkPacketHandler.h"

OpenSignPacket::OpenSignPacket()
        : mFrontSide(false) {}

void OpenSignPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mPosition);
    stream.putBool(mFrontSide);
}

void OpenSignPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPosition = stream.getBlockPosition();
    mFrontSide = stream.getBool();
}

void OpenSignPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
