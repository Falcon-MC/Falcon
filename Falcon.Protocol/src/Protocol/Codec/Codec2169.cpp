#include "Protocol/Codec/Codec2169.h"

#include "Protocol/Codec/Codec2193.h"
#include "Protocol/MinecraftPackets.h"

namespace {

    std::shared_ptr<const ProtocolCodec> createBefore2193(int protocolVersion) {
        ProtocolCapabilities capabilities = Codec2193::capabilities();
        capabilities.mProtocolVersion = protocolVersion;
        capabilities.mDoubledPresence = true;
        capabilities.mBossEventPlayerId = true;
        capabilities.mItemUseHand = false;
        capabilities.mCameraStartingRotation = false;
        capabilities.mAttributeNoiseAlignment = false;
        capabilities.mMoveDeltaTicks = false;
        capabilities.mPlaySoundRangeAndPosition = false;
        capabilities.mSubChunkHeightMapRows = false;
        capabilities.mDiagnosticsActorPosition = false;

        std::shared_ptr<ProtocolCodec> codec = std::make_shared<ProtocolCodec>(capabilities);
        MinecraftPackets::registerVanillaPackets(*codec);
        Codec2193::removeLaterPackets(*codec);
        codec->removePacket(MinecraftPacketIds::SetPlayerFurnaceOptions);
        codec->removePacket(MinecraftPacketIds::RecordStarted);
        return codec;
    }

}

namespace Codec2169 {

    std::shared_ptr<const ProtocolCodec> create() {
        return createBefore2193(2169);
    }

}

namespace Codec2168 {

    std::shared_ptr<const ProtocolCodec> create() {
        return createBefore2193(2168);
    }

}
