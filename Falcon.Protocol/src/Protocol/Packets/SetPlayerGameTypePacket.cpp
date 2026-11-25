#include "Protocol/Packets/SetPlayerGameTypePacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetPlayerGameTypePacket::SetPlayerGameTypePacket()
        : mGamemode(0) {}

void SetPlayerGameTypePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mGamemode);
}

void SetPlayerGameTypePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mGamemode = stream.getVarInt();
}

void SetPlayerGameTypePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
