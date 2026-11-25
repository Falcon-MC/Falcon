#include "Protocol/Packets/SetHealthPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetHealthPacket::SetHealthPacket()
        : mHealth(0) {}

void SetHealthPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mHealth);
}

void SetHealthPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHealth = stream.getVarInt();
}

void SetHealthPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
