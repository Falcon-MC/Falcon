#include "Protocol/Codec/Codec2259.h"

#include "Protocol/MinecraftPackets.h"

namespace Codec2259 {

    std::shared_ptr<const ProtocolCodec> create() {
        ProtocolCapabilities capabilities;
        capabilities.mProtocolVersion = 2259;

        std::shared_ptr<ProtocolCodec> codec = std::make_shared<ProtocolCodec>(capabilities);
        MinecraftPackets::registerVanillaPackets(*codec);
        return codec;
    }

}
