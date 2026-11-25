#include "Protocol/Packets/EmotePacket.h"

#include "Protocol/NetworkPacketHandler.h"

EmotePacket::EmotePacket() = default;

void EmotePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putString(mEmoteId);
    stream.putUnsignedVarInt(mEmoteDuration);
    stream.putString(mXuid);
    stream.putString(mPlatformId);
    stream.putByte(mFlags);
}

void EmotePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mEmoteId = stream.getString();
    mEmoteDuration = stream.getUnsignedVarInt();
    mXuid = stream.getString();
    mPlatformId = stream.getString();
    mFlags = stream.getByte();
}

void EmotePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
