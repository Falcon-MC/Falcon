#include "Protocol/Packets/RequestPermissionsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RequestPermissionsPacket::RequestPermissionsPacket() = default;

void RequestPermissionsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mUniqueActorId);
    stream.putVarInt((int32_t) mPermissions);
    stream.putLShort((uint16_t) mCustomPermissions);
}

void RequestPermissionsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = (int64_t) stream.getLLong();
    mPermissions = (PlayerPermission) stream.getVarInt();
    mCustomPermissions = stream.getLShort();
}

void RequestPermissionsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
