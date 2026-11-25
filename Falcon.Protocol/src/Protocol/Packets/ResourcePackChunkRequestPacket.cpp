#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <cstdio>

namespace {

    std::string formatPackInfo(const Uuid &packId, const std::string &packVersion) {
        std::string info = packId.toString();
        if (!packVersion.empty()) {
            info += '_';
            info += packVersion;
        }
        return info;
    }

    Uuid parsePackId(const std::string &hex) {
        unsigned int a = 0, b = 0, c = 0, d = 0;
        unsigned long long e = 0;
        sscanf(hex.c_str(), "%08x-%04x-%04x-%04x-%012llx", &a, &b, &c, &d, &e);
        uint64_t most = ((uint64_t) a << 32) | ((uint64_t) b << 16) | (uint64_t) c;
        uint64_t least = ((uint64_t) d << 48) | (uint64_t) e;
        return Uuid(most, least);
    }

    void parsePackInfo(const std::string &info, Uuid &packId, std::string &packVersion) {
        size_t separator = info.find('_');
        if (separator == std::string::npos) {
            packId = parsePackId(info);
            packVersion.clear();
        } else {
            packId = parsePackId(info.substr(0, separator));
            packVersion = info.substr(separator + 1);
        }
    }

}

ResourcePackChunkRequestPacket::ResourcePackChunkRequestPacket() = default;

void ResourcePackChunkRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(formatPackInfo(mPackId, mPackVersion));
    stream.putLInt((uint32_t) mChunkIndex);
}

void ResourcePackChunkRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    parsePackInfo(stream.getString(), mPackId, mPackVersion);
    mChunkIndex = (int32_t) stream.getLInt();
}

void ResourcePackChunkRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
