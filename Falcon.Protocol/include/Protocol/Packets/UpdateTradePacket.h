#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class UpdateTradePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateTrade;

    UpdateTradePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateTradePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int8_t mContainerId = 0;
    int8_t mContainerType = 0;
    int32_t mSize = 0;
    int32_t mTradeTier = 0;
    int64_t mTraderUniqueActorId = 0;
    int64_t mPlayerUniqueActorId = 0;
    std::string mDisplayName;
    bool mNewTradingUi = false;
    bool mUsingEconomyTrade = false;
    Tag mOffers;
};
