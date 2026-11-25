#include "Protocol/Packets/CodeBuilderPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CodeBuilderPacket::CodeBuilderPacket()
        : mOpening(false) {}

void CodeBuilderPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mUrl);
    stream.putBool(mOpening);
}

void CodeBuilderPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUrl = stream.getString();
    mOpening = stream.getBool();
}

void CodeBuilderPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
