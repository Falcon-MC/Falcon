#include "Protocol/Packets/ServerboundRequestOnlineMusicPlaybackPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundRequestOnlineMusicPlaybackPacket::ServerboundRequestOnlineMusicPlaybackPacket() = default;

void ServerboundRequestOnlineMusicPlaybackPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt(mRequestId);
    stream.putString(mSoundEventName);
}

void ServerboundRequestOnlineMusicPlaybackPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRequestId = stream.getLInt();
    mSoundEventName = stream.getString();
}

void ServerboundRequestOnlineMusicPlaybackPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
