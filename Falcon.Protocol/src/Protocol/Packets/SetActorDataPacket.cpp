#include "Protocol/Packets/SetActorDataPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

SetActorDataPacket::SetActorDataPacket() = default;

void SetActorDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);
    EntityCodec::writeEntityData(stream, context, mMetadata);
    EntityCodec::writeEntityProperties(stream, mProperties);
    stream.putUnsignedVarLong((uint64_t) mTick);
}

void SetActorDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();
    mMetadata = EntityCodec::readEntityData(stream);
    mProperties = EntityCodec::readEntityProperties(stream);
    mTick = (int64_t) stream.getUnsignedVarLong();
}

void SetActorDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
