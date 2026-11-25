#include "Protocol/Packets/ClientboundOnlineMusicPlaybackResponsePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundOnlineMusicPlaybackResponsePacket::ClientboundOnlineMusicPlaybackResponsePacket() = default;

void ClientboundOnlineMusicPlaybackResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt(mRequestId);
    stream.putString(mState);
    stream.putString(mSharedMetadata);
    stream.putString(mPlaybackContent);
}

void ClientboundOnlineMusicPlaybackResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRequestId = stream.getLInt();
    mState = stream.getString();
    mSharedMetadata = stream.getString();
    mPlaybackContent = stream.getString();
}

void ClientboundOnlineMusicPlaybackResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
