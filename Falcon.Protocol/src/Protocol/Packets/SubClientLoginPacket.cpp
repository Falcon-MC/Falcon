#include "Protocol/Packets/SubClientLoginPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SubClientLoginPacket::SubClientLoginPacket() = default;

void SubClientLoginPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    BinaryStream body;
    body.putLInt((uint32_t) mAuthJwt.size());
    body.put(mAuthJwt);
    body.putLInt((uint32_t) mClientJwt.size());
    body.put(mClientJwt);

    stream.putByteArray(body.getBuffer());
}

void SubClientLoginPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    ReadOnlyBinaryStream body(stream.getByteArray());
    body.setEncodingSettings(stream.getEncodingSettings());

    mAuthJwt = body.get(body.getLInt());
    mClientJwt = body.get(body.getLInt());
}

void SubClientLoginPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
