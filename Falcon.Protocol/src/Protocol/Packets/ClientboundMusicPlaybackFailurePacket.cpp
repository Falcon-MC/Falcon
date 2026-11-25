#include "Protocol/Packets/ClientboundMusicPlaybackFailurePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundMusicPlaybackFailurePacket::ClientboundMusicPlaybackFailurePacket() = default;

void ClientboundMusicPlaybackFailurePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mFailures.size());
    for (const MusicPlaybackFailureEntry &failure: mFailures) {
        stream.putString(failure.mState);
        stream.putString(failure.mDescription);
    }
}

void ClientboundMusicPlaybackFailurePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    const uint32_t count = stream.getArrayLength();
    mFailures.clear();
    mFailures.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        MusicPlaybackFailureEntry failure;
        failure.mState = stream.getString();
        failure.mDescription = stream.getString();
        mFailures.push_back(std::move(failure));
    }
}

void ClientboundMusicPlaybackFailurePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
