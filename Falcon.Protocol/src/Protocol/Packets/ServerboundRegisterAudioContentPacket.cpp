#include "Protocol/Packets/ServerboundRegisterAudioContentPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundRegisterAudioContentPacket::ServerboundRegisterAudioContentPacket() = default;

void ServerboundRegisterAudioContentPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mRegistrations.size());
    for (const AudioContentRegistrationEntry &entry: mRegistrations) {
        stream.putString(entry.mAudioContentId);
        stream.putString(entry.mSharedMetadata);
        stream.putString(entry.mServerContent);
        stream.putString(entry.mPlaybackContent);
    }
}

void ServerboundRegisterAudioContentPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    const uint32_t count = stream.getArrayLength();
    mRegistrations.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        AudioContentRegistrationEntry entry;
        entry.mAudioContentId = stream.getString();
        entry.mSharedMetadata = stream.getString();
        entry.mServerContent = stream.getString();
        entry.mPlaybackContent = stream.getString();
        mRegistrations.push_back(std::move(entry));
    }
}

void ServerboundRegisterAudioContentPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
