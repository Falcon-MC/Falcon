#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace EditorPackType {
    inline constexpr uint8_t CORE = 0x1;
    inline constexpr uint8_t ANNOTATION = 0x2;
}

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
    // EditorPackType bits; 2225 and older send one bool for the editor packs
    uint8_t mEditorPackTypes;
};
