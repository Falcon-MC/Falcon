#include "Protocol/Packets/ChangeMobPropertyPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ChangeMobPropertyPacket::ChangeMobPropertyPacket() = default;

void ChangeMobPropertyPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putString(mProperty);
    stream.putBool(mBoolValue);
    stream.putString(mStringValue);
    stream.putVarInt(mIntValue);
    stream.putLFloat(mFloatValue);
}

void ChangeMobPropertyPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mProperty = stream.getString();
    mBoolValue = stream.getBool();
    mStringValue = stream.getString();
    mIntValue = stream.getVarInt();
    mFloatValue = stream.getLFloat();
}

void ChangeMobPropertyPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
