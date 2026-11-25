#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/BiomeDefinitionData.h"

#include <string>
#include <vector>

class BiomeDefinitionListPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BiomeDefinitionList;

    BiomeDefinitionListPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BiomeDefinitionListPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<BiomeDefinitionData> mBiomes;
    std::vector<std::string> mStrings;
};
