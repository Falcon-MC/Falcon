#pragma once

#include "Protocol/Types/BlockNetworkIdMap.h"

#include <cstdint>
#include <memory>

class BlockNetworkIdTable {
public:
    /**
     * How the server's block states show to a client of this protocol, or nullptr when it knows them all. A state
     * loses the properties the client's version of its block lacks; a block the client lacks shows as its base
     * block when it is a slab or stairs of one, and otherwise as air.
     */
    static std::shared_ptr<const BlockNetworkIdMap> getNetworkIds(int32_t protocol);
};
