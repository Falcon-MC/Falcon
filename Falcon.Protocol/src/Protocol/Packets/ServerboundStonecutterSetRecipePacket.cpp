#include "Protocol/Packets/ServerboundStonecutterSetRecipePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundStonecutterSetRecipePacket::ServerboundStonecutterSetRecipePacket() = default;

void ServerboundStonecutterSetRecipePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte(mContainerId);
    stream.putVarInt(mRecipeIndex);
}

void ServerboundStonecutterSetRecipePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = stream.getByte();
    mRecipeIndex = stream.getVarInt();
}

void ServerboundStonecutterSetRecipePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
