#include "Protocol/Packets/ActorPickRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ActorPickRequestPacket::ActorPickRequestPacket()
        : mRuntimeActorId(0), mHotbarSlot(0), mWithData(false) {}

void ActorPickRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong(mRuntimeActorId);
    stream.putByte((unsigned char) mHotbarSlot);
    stream.putBool(mWithData);
}

void ActorPickRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getLLong();
    mHotbarSlot = stream.getByte();
    mWithData = stream.getBool();
}

void ActorPickRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
