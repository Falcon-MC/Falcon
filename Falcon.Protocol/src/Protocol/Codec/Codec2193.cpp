#include "Protocol/Codec/Codec2193.h"

#include "Protocol/MinecraftPackets.h"

namespace Codec2193 {

    ProtocolCapabilities capabilities() {
        ProtocolCapabilities capabilities;
        capabilities.mProtocolVersion = 2193;
        capabilities.mAttributePayloads = false;
        capabilities.mPassengerOfBlock = false;
        capabilities.mAnimateHand = false;
        capabilities.mDimensionClouds = false;
        capabilities.mTransactionHandOnEveryUse = false;
        capabilities.mCraftReservedAction = false;
        capabilities.mLevelChunkBiomeUpdate = false;
        capabilities.mPlayerListPlayFabId = false;
        capabilities.mStartGameEditorMigration = false;
        capabilities.mEducationAgentCapabilities = false;
        capabilities.mSoundDataUpdateType = false;
        capabilities.mDiagnosticsOptionalFields = false;
        return capabilities;
    }

    void removeLaterPackets(ProtocolCodec &codec) {
        codec.removePacket(MinecraftPacketIds::ClientboundMatchmakingState);
        codec.removePacket(MinecraftPacketIds::ServerboundStonecutterSetRecipe);
        codec.removePacket(MinecraftPacketIds::ClientboundStonecutterSetRecipe);
        codec.removePacket(MinecraftPacketIds::ServerboundMatchmakingCancel);
        codec.removePacket(MinecraftPacketIds::SetPassengerOfBlock);
        codec.removePacket(MinecraftPacketIds::ServerboundCursorItemDrag);
        codec.removePacket(MinecraftPacketIds::ClientboundPlayAudioContent);
        codec.removePacket(MinecraftPacketIds::ServerboundRegisterAudioContent);
    }

    std::shared_ptr<const ProtocolCodec> create() {
        std::shared_ptr<ProtocolCodec> codec = std::make_shared<ProtocolCodec>(capabilities());
        MinecraftPackets::registerVanillaPackets(*codec);
        removeLaterPackets(*codec);
        return codec;
    }

}
