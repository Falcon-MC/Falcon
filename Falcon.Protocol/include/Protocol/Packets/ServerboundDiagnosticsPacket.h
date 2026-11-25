#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/DiagnosticsTypes.h"

#include <vector>

class ServerboundDiagnosticsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundDiagnostics;

    ServerboundDiagnosticsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundDiagnosticsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    float mAvgFps = 0.0f;
    float mAvgServerSimTickTimeMs = 0.0f;
    float mAvgClientSimTickTimeMs = 0.0f;
    float mAvgBeginFrameTimeMs = 0.0f;
    float mAvgInputTimeMs = 0.0f;
    float mAvgRenderTimeMs = 0.0f;
    float mAvgEndFrameTimeMs = 0.0f;
    float mAvgRemainderTimePercent = 0.0f;
    float mAvgUnaccountedTimePercent = 0.0f;
    std::vector<MemoryCategoryCounter> mMemoryCategoryValues;
    std::vector<EntityDiagnosticTimingInfo> mEntityDiagnostics;
    std::vector<SystemDiagnosticTimingInfo> mSystemDiagnostics;
    bool mHasSystemCategories = false;
    std::vector<SystemCategory> mSystemCategories;
    std::vector<WhiskerScopeDataSummary> mWhiskerScopes;
};
