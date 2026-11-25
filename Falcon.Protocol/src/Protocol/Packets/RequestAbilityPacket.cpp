#include "Protocol/Packets/RequestAbilityPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RequestAbilityPacket::RequestAbilityPacket() = default;

void RequestAbilityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mAbility);
    stream.putByte((unsigned char) mType);
    stream.putBool(mBoolValue);
    stream.putLFloat(mFloatValue);
}

void RequestAbilityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAbility = (Ability) stream.getVarInt();
    mType = (AbilityValueType) stream.getByte();
    mBoolValue = stream.getBool();
    mFloatValue = stream.getLFloat();
}

void RequestAbilityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
