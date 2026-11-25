#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/GatheringsConfigurationJoinInfo.h"

#include <cstdint>
#include <string>

class TransferScreenSettings {
public:
    bool mHasTitleText = false;
    std::string mTitleText;
    bool mHasBodyText = false;
    std::string mBodyText;
    bool mHasTitleVisible = false;
    bool mTitleVisible = false;
    bool mHasBodyVisible = false;
    bool mBodyVisible = false;
    bool mHasCancelButtonText = false;
    std::string mCancelButtonText;
    bool mHasCancelButtonVisible = false;
    bool mCancelButtonVisible = false;
    bool mHasCancelButtonDelay = false;
    float mCancelButtonDelaySeconds = 0.0f;
};

class TransferPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Transfer;

    TransferPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "TransferPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mAddress;
    int32_t mPort = 0;
    bool mReloadWorld = false;
    bool mHasGatheringsConfigurationJoinInfo = false;
    GatheringsConfigurationJoinInfo mGatheringsConfigurationJoinInfo;

    // loading screen of a server to server transfer; a world reload ignores it
    bool mHasScreenSettings = false;
    TransferScreenSettings mScreenSettings;
};
