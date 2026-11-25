#include "Protocol/Packets/ResourcePackChunkDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    std::string formatPackInfo(const Uuid &packId, const std::string &packVersion) {
        std::string info = packId.toString();
        if (!packVersion.empty()) {
            info += '_';
            info += packVersion;
        }
        return info;
    }

    void parsePackInfo(const std::string &info, Uuid &packId, std::string &packVersion) {
        size_t separator = info.find('_');
        if (separator == std::string::npos) {
            packId = Uuid::fromString(info);
            packVersion.clear();
        } else {
            packId = Uuid::fromString(info.substr(0, separator));
            packVersion = info.substr(separator + 1);
        }
    }

}

ResourcePackChunkDataPacket::ResourcePackChunkDataPacket() = default;

void ResourcePackChunkDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(formatPackInfo(mPackId, mPackVersion));
    stream.putLInt((uint32_t) mChunkIndex);
    stream.putLLong((uint64_t) mProgress);
    stream.putByteArray(mData);
}

void ResourcePackChunkDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    parsePackInfo(stream.getString(), mPackId, mPackVersion);
    mChunkIndex = (int32_t) stream.getLInt();
    mProgress = (int64_t) stream.getLLong();
    mData = stream.getByteArray();
}

void ResourcePackChunkDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
