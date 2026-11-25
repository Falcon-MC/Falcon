#pragma once

#include "Protocol/Codec/ProtocolCodec.h"

#include <memory>

namespace Codec2193 {

    /**
     * Protocol 2193, Minecraft 1.26.52: the current packets with the format differences described in its
     * capabilities, without the packets added after it.
     */
    std::shared_ptr<const ProtocolCodec> create();

    /**
     * The capabilities of 2193, which the versions before it start from.
     */
    ProtocolCapabilities capabilities();

    /**
     * Removes the packets added after 2193.
     */
    void removeLaterPackets(ProtocolCodec &codec);

}
