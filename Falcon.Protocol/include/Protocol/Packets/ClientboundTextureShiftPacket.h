#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>
#include <vector>

class ClientboundTextureShiftPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundTextureShift;

    enum class Action {
        Invalid,
        Initialize,
        Start,
        SetEnabled,
        Sync,
    };

    ClientboundTextureShiftPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundTextureShiftPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction = Action::Invalid;
    std::string mCollectionName;
    std::string mFromStep;
    std::string mToStep;
    std::vector<std::string> mAllSteps;
    int64_t mCurrentLengthTicks = 0;
    int64_t mTotalLengthTicks = 0;
    bool mEnabled = false;
};
