#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <vector>

class ResourcePackStackPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ResourcePackStack;

    struct Entry {
        std::string mPackId;
        std::string mPackVersion;
        std::string mSubPackName;
    };

    ResourcePackStackPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ResourcePackStackPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mForcedToAccept;
    std::vector<Entry> mResourcePacks;
    std::string mGameVersion;
    std::vector<ExperimentData> mExperiments;
    bool mExperimentsPreviouslyToggled;
    bool mHasEditorPacks;
};
