#include "Protocol/Packets/UnlockedRecipesPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UnlockedRecipesPacket::UnlockedRecipesPacket() = default;

void UnlockedRecipesPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt((uint32_t) mAction);
    stream.putArrayLength((uint32_t) mUnlockedRecipes.size());

    for (const auto &recipe : mUnlockedRecipes) {
        stream.putString(recipe);
    }
}

void UnlockedRecipesPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (ActionType) stream.getLInt();
    const uint32_t count = stream.getArrayLength();
    mUnlockedRecipes.clear();
    mUnlockedRecipes.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        mUnlockedRecipes.push_back(stream.getString());
    }
}

void UnlockedRecipesPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
