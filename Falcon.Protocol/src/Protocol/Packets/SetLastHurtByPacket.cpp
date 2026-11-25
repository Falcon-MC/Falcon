#include "Protocol/Packets/SetLastHurtByPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetLastHurtByPacket::SetLastHurtByPacket()
        : mActorTypeId(0) {}

void SetLastHurtByPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mActorTypeId);
}

void SetLastHurtByPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mActorTypeId = stream.getVarInt();
}

void SetLastHurtByPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
