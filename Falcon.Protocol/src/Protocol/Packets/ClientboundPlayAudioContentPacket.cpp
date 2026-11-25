#include "Protocol/Packets/ClientboundPlayAudioContentPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundPlayAudioContentPacket::ClientboundPlayAudioContentPacket() = default;

void ClientboundPlayAudioContentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mSharedMetadata);
    stream.putString(mPlaybackContent);
    stream.putString(mPlaybackType);
    mSound.write(stream, context);
}

void ClientboundPlayAudioContentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSharedMetadata = stream.getString();
    mPlaybackContent = stream.getString();
    mPlaybackType = stream.getString();
    mSound.read(stream, context);
}

void ClientboundPlayAudioContentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
