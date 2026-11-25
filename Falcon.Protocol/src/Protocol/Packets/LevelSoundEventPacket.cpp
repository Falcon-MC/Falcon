#include "Protocol/Packets/LevelSoundEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LevelSoundEventPacket::LevelSoundEventPacket()
        : mExtraData(-1), mIsBabyMob(false), mDisableRelativeVolume(false), mActorUniqueId(-1),
          mHasFirePosition(false) {}

void LevelSoundEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mSound);
    stream.putVector3f(mPosition);
    stream.putVarInt(mExtraDataIsBlock ? context.toNetworkBlockId(mExtraData) : mExtraData);
    stream.putString(mActorType);
    stream.putBool(mIsBabyMob);
    stream.putBool(mDisableRelativeVolume);
    stream.putLLong((uint64_t) mActorUniqueId);
    stream.putBool(mHasFirePosition);
    if (mHasFirePosition)
        stream.putVector3f(mFirePosition);
}

void LevelSoundEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSound = stream.getString();
    mPosition = stream.getVector3f();
    mExtraData = stream.getVarInt();
    mActorType = stream.getString();
    mIsBabyMob = stream.getBool();
    mDisableRelativeVolume = stream.getBool();
    mActorUniqueId = (int64_t) stream.getLLong();
    mHasFirePosition = stream.getBool();
    if (mHasFirePosition)
        mFirePosition = stream.getVector3f();
}

void LevelSoundEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
