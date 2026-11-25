#include "Protocol/Packets/ResourcePackClientResponsePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    const char *RESPONSE_STATUS[] = {"cancel", "downloading", "downloadingfinished", "resourcepackstackfinished"};

}

ResourcePackClientResponsePacket::ResourcePackClientResponsePacket()
        : mStatus(Status::Refused) {}

void ResourcePackClientResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    const int index = (int) mStatus - 1;
    if (index < 0 || index >= (int) (sizeof(RESPONSE_STATUS) / sizeof(RESPONSE_STATUS[0])))
        throw BinaryDataException("Unsupported resource pack response status");

    stream.putUnsignedVarInt((uint32_t) index);
    stream.putString(RESPONSE_STATUS[index]);

    if (mStatus != Status::SendPacks)
        return;

    stream.putArrayLength((uint32_t) mPackIds.size());
    for (const std::string &packId: mPackIds)
        stream.putString(packId);
}

void ResourcePackClientResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    const uint32_t index = stream.getUnsignedVarInt();
    if (index >= sizeof(RESPONSE_STATUS) / sizeof(RESPONSE_STATUS[0]))
        throw BinaryDataException("Unknown resource pack response status");

    mStatus = (Status) (index + 1);
    stream.getString(); // status name

    mPackIds.clear();
    if (mStatus != Status::SendPacks)
        return;

    const uint32_t count = stream.getArrayLength();
    mPackIds.reserve(count);

    for (uint32_t i = 0; i < count; i++)
        mPackIds.push_back(stream.getString());
}

void ResourcePackClientResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
