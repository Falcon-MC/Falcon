#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <string>

class BlockStateHasher {
public:
    static const int32_t UNKNOWN_BLOCK_HASH = -2;

    static int32_t hash(const std::string &identifier, const Tag &states);

    static int32_t hash(const std::string &identifier);

    static int32_t fnv1a32(const std::string &bytes);
};
