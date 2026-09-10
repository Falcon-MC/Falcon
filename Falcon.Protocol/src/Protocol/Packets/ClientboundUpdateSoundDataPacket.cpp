#include "Protocol/Packets/ClientboundUpdateSoundDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundUpdateSoundDataPacket::ClientboundUpdateSoundDataPacket() = default;

void ClientboundUpdateSoundDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mServerSoundHandle);

    stream.putUnsignedVarInt(0);

    stream.putUnsignedVarInt(0);
    stream.putLFloat(mVolume.mVolume);

    stream.putUnsignedVarInt(0);
    stream.putLFloat(mPitch.mPitch);

    stream.putUnsignedVarInt(0);
    stream.putLFloat(mFade.mTargetVolume);
    stream.putLFloat(mFade.mDuration);

    stream.putUnsignedVarInt(0);
    stream.putLFloat(mSeekTo.mSeconds);

    stream.putUnsignedVarInt(0);

    stream.putUnsignedVarInt(0);
}

void ClientboundUpdateSoundDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mServerSoundHandle = (int64_t) stream.getLLong();

    stream.getUnsignedVarInt();

    stream.getUnsignedVarInt();
    mVolume.mVolume = stream.getLFloat();

    stream.getUnsignedVarInt();
    mPitch.mPitch = stream.getLFloat();

    stream.getUnsignedVarInt();
    mFade.mTargetVolume = stream.getLFloat();
    mFade.mDuration = stream.getLFloat();

    stream.getUnsignedVarInt();
    mSeekTo.mSeconds = stream.getLFloat();

    stream.getUnsignedVarInt();

    stream.getUnsignedVarInt();
}

void ClientboundUpdateSoundDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
