#include "Protocol/Packets/SetPlayerInventoryOptionsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetPlayerInventoryOptionsPacket::SetPlayerInventoryOptionsPacket() = default;

void SetPlayerInventoryOptionsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(static_cast<int32_t>(mLeftTab));
    stream.putVarInt(static_cast<int32_t>(mRightTab));
    stream.putBool(mFiltering);
    stream.putVarInt(static_cast<int32_t>(mLayout));
    stream.putVarInt(static_cast<int32_t>(mCraftingLayout));
}

void SetPlayerInventoryOptionsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mLeftTab = static_cast<InventoryTabLeft>(stream.getVarInt());
    mRightTab = static_cast<InventoryTabRight>(stream.getVarInt());
    mFiltering = stream.getBool();
    mLayout = static_cast<InventoryLayout>(stream.getVarInt());
    mCraftingLayout = static_cast<InventoryLayout>(stream.getVarInt());
}

void SetPlayerInventoryOptionsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
