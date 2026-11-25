#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"

#include <string>
#include <vector>

class ResourcePacksInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ResourcePacksInfo;

    struct Entry {
        Uuid mPackId;
        std::string mPackVersion;
        uint64_t mPackSize = 0;
        std::string mContentKey;
        std::string mSubPackName;
        std::string mContentId;
        bool mScripting = false;
        bool mAddonPack = false;
        bool mRaytracingCapable = false;
        std::string mCdnUrl;
    };

    ResourcePacksInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ResourcePacksInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mForcedToAccept;
    bool mHasAddonPacks;
    bool mScriptingEnabled;
    bool mVibrantVisualsForceDisabled;
    Uuid mWorldTemplateId;
    std::string mWorldTemplateVersion;
    std::vector<Entry> mResourcePackInfos;
};
