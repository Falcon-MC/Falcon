#include "Protocol/Packets/ChangeDimensionPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ChangeDimensionPacket::ChangeDimensionPacket()
        : mDimension(0), mRespawn(false), mHasLoadingScreenId(false), mLoadingScreenId(0) {}

void ChangeDimensionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mDimension);
    stream.putVector3f(mPosition);
    stream.putBool(mRespawn);

    stream.putOptionalPresent(mHasLoadingScreenId);
    if (mHasLoadingScreenId)
        stream.putLInt((uint32_t) mLoadingScreenId);
}

void ChangeDimensionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDimension = stream.getVarInt();
    mPosition = stream.getVector3f();
    mRespawn = stream.getBool();

    mHasLoadingScreenId = stream.getOptionalPresent();
    if (mHasLoadingScreenId)
        mLoadingScreenId = stream.getSignedLInt();
}

void ChangeDimensionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
