#include "Protocol/Packets/UpdateAttributesPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

UpdateAttributesPacket::UpdateAttributesPacket() = default;

void UpdateAttributesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong((uint64_t) mRuntimeActorId);

    stream.putArrayLength((uint32_t) mAttributes.size());
    for (const AttributeData &attribute: mAttributes) {
        EntityCodec::writeAttribute(stream, attribute);
    }

    stream.putUnsignedVarLong((uint64_t) mTick);
}

void UpdateAttributesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = (int64_t) stream.getUnsignedVarLong();

    uint32_t count = stream.getArrayLength();
    mAttributes.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mAttributes.push_back(EntityCodec::readAttribute(stream));
    }

    mTick = (int64_t) stream.getUnsignedVarLong();
}

void UpdateAttributesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
