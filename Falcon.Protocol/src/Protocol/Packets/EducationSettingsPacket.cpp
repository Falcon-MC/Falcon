#include "Protocol/Packets/EducationSettingsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

EducationSettingsPacket::EducationSettingsPacket() = default;

void EducationSettingsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCodeBuilderUri);
    stream.putString(mCodeBuilderTitle);
    stream.putBool(mCanResizeCodeBuilder);
    stream.putBool(mDisableLegacyTitle);
    stream.putString(mPostProcessFilter);
    stream.putString(mScreenshotBorderPath);

    if (context.getCapabilities().mEducationAgentCapabilities) {
        stream.putOptionalPresent(mHasAgentCapabilities);
        if (mHasAgentCapabilities) {
            stream.putOptionalPresent(mHasCanModifyBlocks);
            if (mHasCanModifyBlocks)
                stream.putBool(mCanModifyBlocks);
        }
    } else {
        const bool canModifyBlocks = mHasAgentCapabilities && mHasCanModifyBlocks;
        stream.putOptionalPresent(canModifyBlocks);
        if (canModifyBlocks)
            stream.putBool(mCanModifyBlocks);
    }

    stream.putOptionalPresent(mHasOverrideUri);
    if (mHasOverrideUri)
        stream.putString(mOverrideUri);

    stream.putBool(mQuizAttached);

    stream.putOptionalPresent(mHasExternalLinkSettings);
    if (mHasExternalLinkSettings)
        stream.putBool(mExternalLinkSettings);
}

void EducationSettingsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCodeBuilderUri = stream.getString();
    mCodeBuilderTitle = stream.getString();
    mCanResizeCodeBuilder = stream.getBool();
    mDisableLegacyTitle = stream.getBool();
    mPostProcessFilter = stream.getString();
    mScreenshotBorderPath = stream.getString();

    mHasAgentCapabilities = stream.getOptionalPresent();
    mHasCanModifyBlocks = false;
    if (!context.getCapabilities().mEducationAgentCapabilities) {
        mHasCanModifyBlocks = mHasAgentCapabilities;
        if (mHasCanModifyBlocks)
            mCanModifyBlocks = stream.getBool();
    } else if (mHasAgentCapabilities) {
        mHasCanModifyBlocks = stream.getOptionalPresent();
        if (mHasCanModifyBlocks)
            mCanModifyBlocks = stream.getBool();
    }

    mHasOverrideUri = stream.getOptionalPresent();
    if (mHasOverrideUri)
        mOverrideUri = stream.getString();

    mQuizAttached = stream.getBool();

    mHasExternalLinkSettings = stream.getOptionalPresent();
    if (mHasExternalLinkSettings)
        mExternalLinkSettings = stream.getBool();
}

void EducationSettingsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
