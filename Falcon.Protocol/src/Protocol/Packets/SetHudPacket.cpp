#include "Protocol/Packets/SetHudPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetHudPacket::SetHudPacket() = default;

void SetHudPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mElements.size());
    for (int32_t element: mElements)
        stream.putVarInt(element);

    stream.putVarInt((int32_t) mVisibility);
}

void SetHudPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mElements.clear();
    uint32_t count = stream.getArrayLength();
    for (uint32_t i = 0; i < count; i++)
        mElements.push_back(stream.getVarInt());

    mVisibility = (HudVisibility) stream.getVarInt();
}

void SetHudPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
