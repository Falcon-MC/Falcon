#pragma once

#include "Protocol/Types/BlockNetworkIdMap.h"

#include <cstdint>
#include <memory>

class BlockNetworkIdTable {
public:
    // nullptr: the client knows every state; else unknown properties are dropped and an unknown block
    // shows as its base block (slabs, stairs) or as air
    static std::shared_ptr<const BlockNetworkIdMap> getNetworkIds(int32_t protocol);
};
