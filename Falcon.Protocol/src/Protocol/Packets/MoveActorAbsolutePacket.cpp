#include "Protocol/Packets/MoveActorAbsolutePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {
    const int FLAG_ON_GROUND = 0x1;
    const int FLAG_TELEPORTED = 0x2;
    const int FLAG_FORCE_MOVE = 0x4;
    const int FLAG_FORCE_COMPLETION = 0x8;
}

MoveActorAbsolutePacket::MoveActorAbsolutePacket() = default;

void MoveActorAbsolutePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);

    int flags = 0;
    if (mOnGround) flags |= FLAG_ON_GROUND;
    if (mTeleported) flags |= FLAG_TELEPORTED;
    if (mForceMove) flags |= FLAG_FORCE_MOVE;
    if (mForceCompletion) flags |= FLAG_FORCE_COMPLETION;
    stream.putByte((unsigned char) flags);

    stream.putVector3f(mPosition);

    stream.putByte((unsigned char) (mRotation.x * 256.0f / 360.0f));
    stream.putByte((unsigned char) (mRotation.y * 256.0f / 360.0f));
    stream.putByte((unsigned char) (mRotation.z * 256.0f / 360.0f));
}

void MoveActorAbsolutePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();

    int flags = stream.getByte();
    mOnGround = (flags & FLAG_ON_GROUND) != 0;
    mTeleported = (flags & FLAG_TELEPORTED) != 0;
    mForceMove = (flags & FLAG_FORCE_MOVE) != 0;
    mForceCompletion = (flags & FLAG_FORCE_COMPLETION) != 0;

    mPosition = stream.getVector3f();

    float x = stream.getByte() * 360.0f / 256.0f;
    float y = stream.getByte() * 360.0f / 256.0f;
    float z = stream.getByte() * 360.0f / 256.0f;
    mRotation = Vector3f(x, y, z);
}

void MoveActorAbsolutePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
