#include "Protocol/Packets/UpdateClientInputLocksPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateClientInputLocksPacket::UpdateClientInputLocksPacket() = default;

void UpdateClientInputLocksPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mLockComponentData);
}

void UpdateClientInputLocksPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mLockComponentData = (int32_t) stream.getUnsignedVarInt();
}

void UpdateClientInputLocksPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
