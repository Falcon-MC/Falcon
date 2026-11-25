#include "Protocol/Packets/GuiDataPickItemPacket.h"

#include "Protocol/NetworkPacketHandler.h"

GuiDataPickItemPacket::GuiDataPickItemPacket() = default;

void GuiDataPickItemPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mDescription);
    stream.putString(mItemEffects);
    stream.putLInt((uint32_t) mHotbarSlot);
}

void GuiDataPickItemPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDescription = stream.getString();
    mItemEffects = stream.getString();
    mHotbarSlot = (int32_t) stream.getLInt();
}

void GuiDataPickItemPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
