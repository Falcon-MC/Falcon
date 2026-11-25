#include "Protocol/Packets/SetDifficultyPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetDifficultyPacket::SetDifficultyPacket()
        : mDifficulty(0) {}

void SetDifficultyPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt(mDifficulty);
}

void SetDifficultyPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDifficulty = stream.getUnsignedVarInt();
}

void SetDifficultyPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
