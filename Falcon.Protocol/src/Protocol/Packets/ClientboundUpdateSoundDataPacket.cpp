#include "Protocol/Packets/ClientboundUpdateSoundDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundUpdateSoundDataPacket::ClientboundUpdateSoundDataPacket() = default;

void ClientboundUpdateSoundDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mServerSoundHandle);
    if (!context.getCapabilities().mSoundDataUpdateType) {
        writeEveryUpdate(stream);
        return;
    }
    stream.putByte((unsigned char) mType);

    switch (mType) {
        case SoundDataUpdateType::SetVolume:
            stream.putLFloat(mVolume.mVolume);
            break;
        case SoundDataUpdateType::SetPitch:
            stream.putLFloat(mPitch.mPitch);
            break;
        case SoundDataUpdateType::Fade:
            stream.putLFloat(mFade.mTargetVolume);
            stream.putLFloat(mFade.mDuration);
            break;
        case SoundDataUpdateType::SeekTo:
            stream.putLFloat(mSeekTo.mSeconds);
            break;
        default:
            break;
    }
}

void ClientboundUpdateSoundDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mServerSoundHandle = (int64_t) stream.getLLong();
    if (!context.getCapabilities().mSoundDataUpdateType) {
        readEveryUpdate(stream);
        return;
    }
    mType = (SoundDataUpdateType) stream.getByte();

    switch (mType) {
        case SoundDataUpdateType::SetVolume:
            mVolume.mVolume = stream.getLFloat();
            break;
        case SoundDataUpdateType::SetPitch:
            mPitch.mPitch = stream.getLFloat();
            break;
        case SoundDataUpdateType::Fade:
            mFade.mTargetVolume = stream.getLFloat();
            mFade.mDuration = stream.getLFloat();
            break;
        case SoundDataUpdateType::SeekTo:
            mSeekTo.mSeconds = stream.getLFloat();
            break;
        default:
            break;
    }
}

void ClientboundUpdateSoundDataPacket::writeEveryUpdate(BinaryStream &stream) const {
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

void ClientboundUpdateSoundDataPacket::readEveryUpdate(ReadOnlyBinaryStream &stream) {
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
