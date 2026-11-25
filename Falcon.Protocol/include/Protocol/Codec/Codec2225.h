#pragma once

#include "Protocol/Codec/ProtocolCodec.h"

#include <memory>

namespace Codec2225 {

    /**
     * Protocol 2225, Minecraft 1.26.60: the current packets with the format differences described in its
     * capabilities, without the packets added after it and with the packets removed after it.
     */
    std::shared_ptr<const ProtocolCodec> create();

    /**
     * The capabilities of 2225, which the versions before it start from.
     */
    ProtocolCapabilities capabilities();

    /**
     * Fills the codec with the packets 2225 has, which the versions before it start from.
     */
    void registerPackets(ProtocolCodec &codec);

}
