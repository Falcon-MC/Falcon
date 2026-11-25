#include "Protocol/Packets/LevelEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

LevelEventPacket::LevelEventPacket()
        : mEventId(0), mData(0) {}

void LevelEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mEventId);
    stream.putVector3f(mPosition);
    if (mEventId == Event::ParticleDestroy) {
        stream.putVarInt(context.toNetworkBlockId(mData));
    } else if (mEventId == Event::ParticlePunchBlock) {
        stream.putVarInt(context.toNetworkBlockId(mData) | (mBlockFace << 24));
    } else {
        stream.putVarInt(mData);
    }
}

void LevelEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mEventId = stream.getVarInt();
    mPosition = stream.getVector3f();
    mData = stream.getVarInt();
}

void LevelEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
