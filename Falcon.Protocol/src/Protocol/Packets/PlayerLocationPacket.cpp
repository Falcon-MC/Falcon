#include "Protocol/Packets/PlayerLocationPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerLocationPacket::PlayerLocationPacket() = default;

void PlayerLocationPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mTargetActorId);
    stream.putUnsignedVarInt((uint32_t) mType);
    stream.putVarInt(0);

    if (mType == PlayerLocationType::Coordinates)
        stream.putVector3f(mPosition);
}

void PlayerLocationPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mTargetActorId = stream.getVarLong();
    mType = (PlayerLocationType) stream.getUnsignedVarInt();
    stream.getVarInt();

    if (mType == PlayerLocationType::Coordinates)
        mPosition = stream.getVector3f();
}

void PlayerLocationPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
