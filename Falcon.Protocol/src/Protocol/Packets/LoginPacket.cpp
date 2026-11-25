#include "Protocol/Packets/LoginPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LoginPacket::LoginPacket()
        : mProtocolVersion(0) {}

void LoginPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putInt((uint32_t) mProtocolVersion);

    // both jwts live inside a single length prefixed blob, each with its own little endian length
    BinaryStream body;
    body.putLInt((uint32_t) mAuthJwt.size());
    body.put(mAuthJwt);
    body.putLInt((uint32_t) mClientJwt.size());
    body.put(mClientJwt);

    stream.putByteArray(body.getBuffer());
}

void LoginPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mProtocolVersion = stream.getSignedInt();

    ReadOnlyBinaryStream body(stream.getByteArray());
    body.setEncodingSettings(stream.getEncodingSettings());

    mAuthJwt = body.get(body.getLInt());
    mClientJwt = body.get(body.getLInt());
}

void LoginPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
