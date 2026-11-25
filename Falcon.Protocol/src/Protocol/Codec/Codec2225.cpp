#include "Protocol/Codec/Codec2225.h"

#include "Protocol/MinecraftPackets.h"
#include "Protocol/Packets/AddVolumeEntityPacket.h"
#include "Protocol/Packets/RemoveVolumeEntityPacket.h"

namespace Codec2225 {

    ProtocolCapabilities capabilities() {
        ProtocolCapabilities capabilities;
        capabilities.mProtocolVersion = 2225;
        capabilities.mPlaySoundStartDelay = false;
        capabilities.mPassengerOfBlockType = false;
        capabilities.mTextQueue = false;
        capabilities.mStructureKeepVoid = false;
        capabilities.mTransferScreenSettings = false;
        capabilities.mManagedChat = false;
        capabilities.mCommandRawMessage = false;
        capabilities.mEditorPackTypes = false;
        capabilities.mDataStoreVariables = false;
        capabilities.mPlaySoundAttach = false;
        capabilities.mPassengerOfBlockPack = false;
        capabilities.mSctpRecoveryProfile = false;
        return capabilities;
    }

    void registerPackets(ProtocolCodec &codec) {
        MinecraftPackets::registerVanillaPackets(codec);
        codec.registerPacket<AddVolumeEntityPacket>();
        codec.registerPacket<RemoveVolumeEntityPacket>();
        codec.removePacket(MinecraftPacketIds::Chat);
        codec.removePacket(MinecraftPacketIds::ServerboundRequestOnlineMusicPlayback);
        codec.removePacket(MinecraftPacketIds::ClientboundOnlineMusicPlaybackResponse);
        codec.removePacket(MinecraftPacketIds::PlayerToggleArmorVisibilityRequest);
        codec.removePacket(MinecraftPacketIds::ClientboundMusicPlaybackFailure);
        codec.removePacket(MinecraftPacketIds::VirtualContainerOpen);
        codec.removePacket(MinecraftPacketIds::VirtualContainerClose);
    }

    std::shared_ptr<const ProtocolCodec> create() {
        std::shared_ptr<ProtocolCodec> codec = std::make_shared<ProtocolCodec>(capabilities());
        registerPackets(*codec);
        return codec;
    }

}
