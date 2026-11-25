#include "Protocol/BlockStateHasher.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"

#include <algorithm>
#include <vector>

namespace {
    const uint32_t FNV1_32_INIT = 0x811c9dc5u;
    const uint32_t FNV1_PRIME_32 = 16777619u;
    const char *UNKNOWN_IDENTIFIER = "minecraft:unknown";

    Tag sortedStates(const Tag &states) {
        if (states.getType() != Tag::Type::Compound)
            return Tag::ofCompound();

        std::vector<std::string> keys = states.getKeys();
        std::sort(keys.begin(), keys.end());

        Tag sorted = Tag::ofCompound();
        for (const std::string &key: keys)
            sorted.put(key, *states.get(key));

        return sorted;
    }
}

int32_t BlockStateHasher::fnv1a32(const std::string &bytes) {
    uint32_t hash = FNV1_32_INIT;

    for (char byte: bytes) {
        hash ^= (uint32_t) (unsigned char) byte;
        hash *= FNV1_PRIME_32;
    }

    return (int32_t) hash;
}

int32_t BlockStateHasher::hash(const std::string &identifier, const Tag &states) {
    if (identifier == UNKNOWN_IDENTIFIER)
        return UNKNOWN_BLOCK_HASH;

    Tag blockState = Tag::ofCompound();
    blockState.putString("name", identifier);
    blockState.put("states", sortedStates(states));

    BinaryStream stream;
    NbtIo::writeTag(stream, blockState, NbtVariant::LittleEndian);

    return fnv1a32(stream.getBuffer());
}

int32_t BlockStateHasher::hash(const std::string &identifier) {
    return hash(identifier, Tag::ofCompound());
}
