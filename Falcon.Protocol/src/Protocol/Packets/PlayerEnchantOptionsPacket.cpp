#include "Protocol/Packets/PlayerEnchantOptionsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeEnchant(BinaryStream &stream, const EnchantEntry &enchant) {
        stream.putUnsignedVarInt((uint32_t) enchant.mType);
        stream.putByte((unsigned char) enchant.mLevel);
    }

    EnchantEntry readEnchant(ReadOnlyBinaryStream &stream) {
        EnchantEntry enchant;
        enchant.mType = (int32_t) stream.getUnsignedVarInt();
        enchant.mLevel = stream.getByte();
        return enchant;
    }

    void writeEnchantList(BinaryStream &stream, const std::vector<EnchantEntry> &enchants) {
        stream.putArrayLength((uint32_t) enchants.size());
        for (const EnchantEntry &enchant: enchants) {
            writeEnchant(stream, enchant);
        }
    }

    std::vector<EnchantEntry> readEnchantList(ReadOnlyBinaryStream &stream) {
        uint32_t count = stream.getArrayLength();
        std::vector<EnchantEntry> enchants;
        enchants.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            enchants.push_back(readEnchant(stream));
        }
        return enchants;
    }

}

PlayerEnchantOptionsPacket::PlayerEnchantOptionsPacket() = default;

void PlayerEnchantOptionsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mOptions.size());
    for (const EnchantOptionData &option: mOptions) {
        stream.putByte((unsigned char) option.mCost);
        stream.putLInt((uint32_t) option.mPrimarySlot);
        writeEnchantList(stream, option.mEnchants0);
        writeEnchantList(stream, option.mEnchants1);
        writeEnchantList(stream, option.mEnchants2);
        stream.putString(option.mEnchantName);
        stream.putUnsignedVarInt((uint32_t) option.mEnchantNetId);
    }
}

void PlayerEnchantOptionsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t count = stream.getArrayLength();
    mOptions.clear();
    mOptions.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        EnchantOptionData option;
        option.mCost = stream.getByte();
        option.mPrimarySlot = (int32_t) stream.getLInt();
        option.mEnchants0 = readEnchantList(stream);
        option.mEnchants1 = readEnchantList(stream);
        option.mEnchants2 = readEnchantList(stream);
        option.mEnchantName = stream.getString();
        option.mEnchantNetId = (int32_t) stream.getUnsignedVarInt();
        mOptions.push_back(option);
    }
}

void PlayerEnchantOptionsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
