#include "Protocol/Packets/ClientboundStonecutterSetRecipePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundStonecutterSetRecipePacket::ClientboundStonecutterSetRecipePacket() = default;

void ClientboundStonecutterSetRecipePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mPlayerUniqueId);
    stream.putByte(mContainerId);
    stream.putVarInt(mRecipeIndex);
}

void ClientboundStonecutterSetRecipePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPlayerUniqueId = stream.getVarLong();
    mContainerId = stream.getByte();
    mRecipeIndex = stream.getVarInt();
}

void ClientboundStonecutterSetRecipePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
