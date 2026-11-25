#include "Protocol/Packets/ClientCacheStatusPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientCacheStatusPacket::ClientCacheStatusPacket()
        : mSupported(false) {}

void ClientCacheStatusPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mSupported);
}

void ClientCacheStatusPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSupported = stream.getBool();
}

void ClientCacheStatusPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
