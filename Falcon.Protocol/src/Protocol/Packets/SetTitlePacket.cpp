#include "Protocol/Packets/SetTitlePacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetTitlePacket::SetTitlePacket()
        : mType(Type::Clear), mFadeInTime(0), mStayTime(0), mFadeOutTime(0) {}

void SetTitlePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mType);
    stream.putString(mText);
    stream.putVarInt(mFadeInTime);
    stream.putVarInt(mStayTime);
    stream.putVarInt(mFadeOutTime);
    stream.putString(mXuid);
    stream.putString(mPlatformOnlineId);
    stream.putString(mFilteredTitleText);
}

void SetTitlePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mType = (Type) stream.getVarInt();
    mText = stream.getString();
    mFadeInTime = stream.getVarInt();
    mStayTime = stream.getVarInt();
    mFadeOutTime = stream.getVarInt();
    mXuid = stream.getString();
    mPlatformOnlineId = stream.getString();
    mFilteredTitleText = stream.getString();
}

void SetTitlePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
