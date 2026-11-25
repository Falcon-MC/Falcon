#include "Protocol/Packets/AnimateEntityPacket.h"

#include "Protocol/NetworkPacketHandler.h"

AnimateEntityPacket::AnimateEntityPacket() = default;

void AnimateEntityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mAnimation);
    stream.putString(mNextState);
    stream.putString(mStopExpression);
    stream.putLInt((uint32_t) mStopExpressionVersion);
    stream.putString(mController);
    stream.putLFloat(mBlendOutTime);

    stream.putUnsignedVarInt((uint32_t) mRuntimeActorIds.size());
    for (uint64_t runtimeId: mRuntimeActorIds) {
        stream.putUnsignedVarLong(runtimeId);
    }
}

void AnimateEntityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAnimation = stream.getString();
    mNextState = stream.getString();
    mStopExpression = stream.getString();
    mStopExpressionVersion = (int32_t) stream.getLInt();
    mController = stream.getString();
    mBlendOutTime = stream.getLFloat();

    uint32_t count = stream.getUnsignedVarInt();
    mRuntimeActorIds.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mRuntimeActorIds.push_back(stream.getUnsignedVarLong());
    }
}

void AnimateEntityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
