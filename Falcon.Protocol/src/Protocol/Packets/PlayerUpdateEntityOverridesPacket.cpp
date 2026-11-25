#include "Protocol/Packets/PlayerUpdateEntityOverridesPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerUpdateEntityOverridesPacket::PlayerUpdateEntityOverridesPacket() = default;

void PlayerUpdateEntityOverridesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mActorUniqueId);
    stream.putUnsignedVarInt((uint32_t) mPropertyIndex);
    stream.putUnsignedVarInt((uint32_t) mUpdateType);
    stream.putByte((unsigned char) mUpdateType);

    if (mUpdateType == PlayerUpdateEntityOverrideType::SetIntOverride) {
        stream.putLInt((uint32_t) mIntValue);
    } else if (mUpdateType == PlayerUpdateEntityOverrideType::SetFloatOverride) {
        stream.putLFloat(mFloatValue);
    }
}

void PlayerUpdateEntityOverridesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mActorUniqueId = stream.getVarLong();
    mPropertyIndex = (int32_t) stream.getUnsignedVarInt();

    mUpdateType = (PlayerUpdateEntityOverrideType) stream.getUnsignedVarInt();
    stream.getByte();

    if (mUpdateType == PlayerUpdateEntityOverrideType::SetIntOverride) {
        mIntValue = (int32_t) stream.getLInt();
    } else if (mUpdateType == PlayerUpdateEntityOverrideType::SetFloatOverride) {
        mFloatValue = stream.getLFloat();
    }
}

void PlayerUpdateEntityOverridesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
