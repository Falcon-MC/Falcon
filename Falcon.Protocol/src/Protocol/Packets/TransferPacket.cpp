#include "Protocol/Packets/TransferPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeScreenSettings(BinaryStream &stream, const TransferScreenSettings &settings) {
        stream.putOptionalPresent(settings.mHasTitleText);
        if (settings.mHasTitleText)
            stream.putString(settings.mTitleText);
        stream.putOptionalPresent(settings.mHasBodyText);
        if (settings.mHasBodyText)
            stream.putString(settings.mBodyText);
        stream.putOptionalPresent(settings.mHasTitleVisible);
        if (settings.mHasTitleVisible)
            stream.putBool(settings.mTitleVisible);
        stream.putOptionalPresent(settings.mHasBodyVisible);
        if (settings.mHasBodyVisible)
            stream.putBool(settings.mBodyVisible);
        stream.putOptionalPresent(settings.mHasCancelButtonText);
        if (settings.mHasCancelButtonText)
            stream.putString(settings.mCancelButtonText);
        stream.putOptionalPresent(settings.mHasCancelButtonVisible);
        if (settings.mHasCancelButtonVisible)
            stream.putBool(settings.mCancelButtonVisible);
        stream.putOptionalPresent(settings.mHasCancelButtonDelay);
        if (settings.mHasCancelButtonDelay)
            stream.putLFloat(settings.mCancelButtonDelaySeconds);
    }

    TransferScreenSettings readScreenSettings(ReadOnlyBinaryStream &stream) {
        TransferScreenSettings settings;
        settings.mHasTitleText = stream.getOptionalPresent();
        if (settings.mHasTitleText)
            settings.mTitleText = stream.getString();
        settings.mHasBodyText = stream.getOptionalPresent();
        if (settings.mHasBodyText)
            settings.mBodyText = stream.getString();
        settings.mHasTitleVisible = stream.getOptionalPresent();
        if (settings.mHasTitleVisible)
            settings.mTitleVisible = stream.getBool();
        settings.mHasBodyVisible = stream.getOptionalPresent();
        if (settings.mHasBodyVisible)
            settings.mBodyVisible = stream.getBool();
        settings.mHasCancelButtonText = stream.getOptionalPresent();
        if (settings.mHasCancelButtonText)
            settings.mCancelButtonText = stream.getString();
        settings.mHasCancelButtonVisible = stream.getOptionalPresent();
        if (settings.mHasCancelButtonVisible)
            settings.mCancelButtonVisible = stream.getBool();
        settings.mHasCancelButtonDelay = stream.getOptionalPresent();
        if (settings.mHasCancelButtonDelay)
            settings.mCancelButtonDelaySeconds = stream.getLFloat();
        return settings;
    }

}

TransferPacket::TransferPacket() = default;

void TransferPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mAddress);
    stream.putLShort((uint16_t) mPort);
    stream.putBool(mReloadWorld);

    stream.putOptionalPresent(mHasGatheringsConfigurationJoinInfo);
    if (mHasGatheringsConfigurationJoinInfo)
        mGatheringsConfigurationJoinInfo.write(stream);

    if (context.getCapabilities().mTransferScreenSettings) {
        stream.putOptionalPresent(mHasScreenSettings);
        if (mHasScreenSettings)
            writeScreenSettings(stream, mScreenSettings);
    }
}

void TransferPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAddress = stream.getString();
    mPort = stream.getLShort();
    mReloadWorld = stream.getBool();

    mHasGatheringsConfigurationJoinInfo = stream.getOptionalPresent();
    if (mHasGatheringsConfigurationJoinInfo)
        mGatheringsConfigurationJoinInfo.read(stream);

    mHasScreenSettings = context.getCapabilities().mTransferScreenSettings && stream.getOptionalPresent();
    mScreenSettings = mHasScreenSettings ? readScreenSettings(stream) : TransferScreenSettings();
}

void TransferPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
