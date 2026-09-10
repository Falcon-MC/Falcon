#include "Protocol/Packets/PlaySoundPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlaySoundPacket::PlaySoundPacket() = default;

void PlaySoundPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mSound);

    Vector3i blockPosition((int32_t) (mPosition.x * 8.0f), (int32_t) (mPosition.y * 8.0f), (int32_t) (mPosition.z * 8.0f));
    stream.putBlockPosition(blockPosition);

    stream.putLFloat(mVolume);
    stream.putLFloat(mPitch);

    stream.putUnsignedVarInt((uint32_t) mLoopCount);

    stream.putBool(mBypassListenerRangeCheck);

    stream.putOptionalPresent(mHasServerSoundHandle);
    if (mHasServerSoundHandle)
        stream.putLLong((uint64_t) mServerSoundHandle);

    stream.putOptionalPresent(mHasPlaybackPosition);
    if (mHasPlaybackPosition)
        stream.putLFloat(mPlaybackPositionSeconds);
}

void PlaySoundPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSound = stream.getString();

    Vector3i blockPosition = stream.getBlockPosition();
    mPosition = Vector3f(blockPosition.x / 8.0f, blockPosition.y / 8.0f, blockPosition.z / 8.0f);

    mVolume = stream.getLFloat();
    mPitch = stream.getLFloat();

    mLoopCount = (int32_t) stream.getUnsignedVarInt();

    mBypassListenerRangeCheck = stream.getBool();

    mHasServerSoundHandle = stream.getOptionalPresent();
    if (mHasServerSoundHandle)
        mServerSoundHandle = (int64_t) stream.getLLong();

    mHasPlaybackPosition = stream.getOptionalPresent();
    if (mHasPlaybackPosition)
        mPlaybackPositionSeconds = stream.getLFloat();
}

void PlaySoundPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
