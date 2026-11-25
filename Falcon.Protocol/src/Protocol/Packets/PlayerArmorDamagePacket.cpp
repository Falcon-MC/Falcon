#include "Protocol/Packets/PlayerArmorDamagePacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerArmorDamagePacket::PlayerArmorDamagePacket() = default;

void PlayerArmorDamagePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mFlags.size());

    for (const PlayerArmorDamageFlag flag : mFlags) {
        const int32_t ordinal = static_cast<int32_t>(flag);
        stream.putVarInt(ordinal);
        stream.putLShort((uint16_t) mDamage[ordinal]);
    }
}

void PlayerArmorDamagePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mFlags.clear();
    mDamage = {0, 0, 0, 0, 0};

    const uint32_t length = stream.getArrayLength();

    for (uint32_t i = 0; i < length; i++) {
        const int32_t ordinal = stream.getVarInt();
        const int16_t damage = stream.getSignedLShort();

        if (ordinal >= 0 && ordinal < (int32_t) mDamage.size()) {
            mFlags.push_back(static_cast<PlayerArmorDamageFlag>(ordinal));
            mDamage[ordinal] = damage;
        }
    }
}

void PlayerArmorDamagePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
