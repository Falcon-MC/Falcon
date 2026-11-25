#include "Protocol/Packets/DeathInfoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

DeathInfoPacket::DeathInfoPacket() = default;

void DeathInfoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCauseAttackName);

    stream.putArrayLength((uint32_t) mMessageList.size());
    for (const std::string &message : mMessageList) {
        stream.putString(message);
    }
}

void DeathInfoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCauseAttackName = stream.getString();

    uint32_t count = stream.getArrayLength();
    mMessageList.clear();
    mMessageList.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mMessageList.push_back(stream.getString());
    }
}

void DeathInfoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
