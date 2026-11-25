#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/GraphicsOverrideParameterType.h"
#include "Protocol/Types/GraphicsOverrideValueEntry.h"

#include <string>
#include <vector>

class GraphicsOverrideParameterPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::GraphicsOverrideParameter;

    GraphicsOverrideParameterPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "GraphicsOverrideParameterPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<GraphicsOverrideValueEntry> mValues;

    bool mHasFloatValue = false;
    float mFloatValue = 0.0f;

    bool mHasVec3Value = false;
    Vector3f mVec3Value;

    std::string mBiomeIdentifier;

    bool mHasPlayerIdentifier = false;
    std::string mPlayerIdentifier;

    GraphicsOverrideParameterType mParameterType = GraphicsOverrideParameterType::SkyZenithColor;
    bool mReset = false;
};
