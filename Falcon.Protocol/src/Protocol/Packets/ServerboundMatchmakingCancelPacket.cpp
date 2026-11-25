#include "Protocol/Packets/ServerboundMatchmakingCancelPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundMatchmakingCancelPacket::ServerboundMatchmakingCancelPacket() = default;

void ServerboundMatchmakingCancelPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
}

void ServerboundMatchmakingCancelPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
}

void ServerboundMatchmakingCancelPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
