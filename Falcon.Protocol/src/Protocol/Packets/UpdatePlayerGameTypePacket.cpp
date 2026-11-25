#include "Protocol/Packets/UpdatePlayerGameTypePacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdatePlayerGameTypePacket::UpdatePlayerGameTypePacket() = default;

void UpdatePlayerGameTypePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mGameType);
    stream.putVarLong(mActorId);
    stream.putUnsignedVarLong((uint64_t) mTick);
}

void UpdatePlayerGameTypePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mGameType = (GameType) stream.getVarInt();
    mActorId = stream.getVarLong();
    mTick = (int64_t) stream.getUnsignedVarLong();
}

void UpdatePlayerGameTypePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
