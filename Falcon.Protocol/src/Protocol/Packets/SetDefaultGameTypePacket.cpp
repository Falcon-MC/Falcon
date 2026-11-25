#include "Protocol/Packets/SetDefaultGameTypePacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetDefaultGameTypePacket::SetDefaultGameTypePacket()
        : mGamemode(0) {}

void SetDefaultGameTypePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mGamemode);
}

void SetDefaultGameTypePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mGamemode = stream.getVarInt();
}

void SetDefaultGameTypePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
