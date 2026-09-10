#include "Protocol/Packets/SetPlayerFurnaceOptionsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetPlayerFurnaceOptionsPacket::SetPlayerFurnaceOptionsPacket() = default;

void SetPlayerFurnaceOptionsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mFurnaceType);
    stream.putVarInt(static_cast<int32_t>(mLeftTab));
    stream.putBool(mFiltering);
    stream.putVarInt(static_cast<int32_t>(mLayout));
}

void SetPlayerFurnaceOptionsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mFurnaceType = static_cast<FurnaceType>(stream.getByte());
    mLeftTab = static_cast<FurnaceTabLeft>(stream.getVarInt());
    mFiltering = stream.getBool();
    mLayout = static_cast<FurnaceLayout>(stream.getVarInt());
}

void SetPlayerFurnaceOptionsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
