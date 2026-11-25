#include "Protocol/Packets/UpdateTradePacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

UpdateTradePacket::UpdateTradePacket()
        : mOffers(Tag::ofCompound()) {}

void UpdateTradePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mContainerId);
    stream.putByte((unsigned char) mContainerType);
    stream.putVarInt(mSize);
    stream.putVarInt(mTradeTier);
    stream.putVarLong(mTraderUniqueActorId);
    stream.putVarLong(mPlayerUniqueActorId);
    stream.putString(mDisplayName);
    stream.putBool(mNewTradingUi);
    stream.putBool(mUsingEconomyTrade);
    NbtIo::writeTag(stream, mOffers, NbtVariant::Network);
}

void UpdateTradePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mContainerId = stream.getSignedByte();
    mContainerType = stream.getSignedByte();
    mSize = stream.getVarInt();
    mTradeTier = stream.getVarInt();
    mTraderUniqueActorId = stream.getVarLong();
    mPlayerUniqueActorId = stream.getVarLong();
    mDisplayName = stream.getString();
    mNewTradingUi = stream.getBool();
    mUsingEconomyTrade = stream.getBool();
    mOffers = NbtIo::readTag(stream, NbtVariant::Network);
}

void UpdateTradePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
