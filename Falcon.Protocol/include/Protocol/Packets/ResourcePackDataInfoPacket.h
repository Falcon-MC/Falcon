#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

enum class ResourcePackType : unsigned char {
    Invalid = 0,
    Addon = 1,
    Cached = 2,
    CopyProtected = 3,
    DataAddOn = 4,
    PersonaPiece = 5,
    Resources = 6,
    Skins = 7,
    WorldTemplate = 8
};

class ResourcePackDataInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ResourcePackDataInfo;

    ResourcePackDataInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ResourcePackDataInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Uuid mPackId;
    std::string mPackVersion;
    int64_t mMaxChunkSize = 0;
    int64_t mChunkCount = 0;
    int64_t mCompressedPackSize = 0;
    std::string mHash;
    bool mPremium = false;
    ResourcePackType mType = ResourcePackType::Invalid;
};
