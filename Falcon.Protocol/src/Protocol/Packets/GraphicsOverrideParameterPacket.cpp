#include "Protocol/Packets/GraphicsOverrideParameterPacket.h"

#include "Protocol/NetworkPacketHandler.h"

GraphicsOverrideParameterPacket::GraphicsOverrideParameterPacket() = default;

void GraphicsOverrideParameterPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mValues.size());
    for (const GraphicsOverrideValueEntry &entry : mValues) {
        stream.putLFloat(entry.mKey);
        stream.putVector3f(entry.mValue);
    }

    stream.putOptionalPresent(mHasFloatValue);
    if (mHasFloatValue)
        stream.putLFloat(mFloatValue);

    stream.putOptionalPresent(mHasVec3Value);
    if (mHasVec3Value)
        stream.putVector3f(mVec3Value);

    stream.putString(mBiomeIdentifier);

    stream.putOptionalPresent(mHasPlayerIdentifier);
    if (mHasPlayerIdentifier)
        stream.putString(mPlayerIdentifier);

    stream.putByte((unsigned char) mParameterType);
    stream.putBool(mReset);
}

void GraphicsOverrideParameterPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t count = stream.getUnsignedVarInt();
    mValues.clear();
    mValues.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        GraphicsOverrideValueEntry entry;
        entry.mKey = stream.getLFloat();
        entry.mValue = stream.getVector3f();
        mValues.push_back(entry);
    }

    mHasFloatValue = stream.getOptionalPresent();
    if (mHasFloatValue)
        mFloatValue = stream.getLFloat();

    mHasVec3Value = stream.getOptionalPresent();
    if (mHasVec3Value)
        mVec3Value = stream.getVector3f();

    mBiomeIdentifier = stream.getString();

    mHasPlayerIdentifier = stream.getOptionalPresent();
    if (mHasPlayerIdentifier)
        mPlayerIdentifier = stream.getString();

    mParameterType = (GraphicsOverrideParameterType) stream.getByte();
    mReset = stream.getBool();
}

void GraphicsOverrideParameterPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
