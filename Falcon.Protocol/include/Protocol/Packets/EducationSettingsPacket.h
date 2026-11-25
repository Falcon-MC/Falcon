#pragma once

#include "Protocol/Packet.h"

#include <string>

class EducationSettingsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::EducationSettings;

    EducationSettingsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EducationSettingsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCodeBuilderUri;
    std::string mCodeBuilderTitle;
    bool mCanResizeCodeBuilder = false;
    bool mDisableLegacyTitle = false;
    std::string mPostProcessFilter;
    std::string mScreenshotBorderPath;
    bool mHasAgentCapabilities = false;
    bool mHasCanModifyBlocks = false;
    bool mCanModifyBlocks = false;
    bool mHasOverrideUri = false;
    std::string mOverrideUri;
    bool mQuizAttached = false;
    bool mHasExternalLinkSettings = false;
    bool mExternalLinkSettings = false;
};
