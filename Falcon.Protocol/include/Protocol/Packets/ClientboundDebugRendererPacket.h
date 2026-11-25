#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

enum class ClientboundDebugRendererType {
    Invalid,
    ClearDebugMarkers,
    AddDebugMarkerCube,
};

class ClientboundDebugRendererPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundDebugRenderer;

    ClientboundDebugRendererPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundDebugRendererPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    ClientboundDebugRendererType mDebugMarkerType = ClientboundDebugRendererType::Invalid;
    std::string mMarkerText;
    Vector3f mMarkerPosition;
    float mMarkerColorRed = 0.0f;
    float mMarkerColorGreen = 0.0f;
    float mMarkerColorBlue = 0.0f;
    float mMarkerColorAlpha = 0.0f;
    int64_t mMarkerDuration = 0;
};
