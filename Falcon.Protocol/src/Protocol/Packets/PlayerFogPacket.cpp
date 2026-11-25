#include "Protocol/Packets/PlayerFogPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerFogPacket::PlayerFogPacket() {}

void PlayerFogPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mFogStack.size());
    for (const std::string &fogEffect: mFogStack)
        stream.putString(fogEffect);
}

void PlayerFogPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    const uint32_t fogCount = stream.getArrayLength();
    mFogStack.clear();
    mFogStack.reserve(fogCount);

    for (uint32_t i = 0; i < fogCount; i++)
        mFogStack.push_back(stream.getString());
}

void PlayerFogPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
