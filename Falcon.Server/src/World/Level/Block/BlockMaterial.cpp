#include "World/Level/Block/BlockMaterial.h"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace {
    struct Entry {
        const char *mIdentifier;
        BlockMaterialType mType;
    };

    // FLAGS: the (type, flags) pairs at 0x14F1ABBD8 (26.60), indexed by type
#include "BlockMaterialTable.inc"

    static_assert(sizeof(FLAGS) == static_cast<size_t>(BlockMaterialType::NonSolid) + 1, "one flag byte per type");

    const std::unordered_map<std::string, BlockMaterialType> &types() {
        static const std::unordered_map<std::string, BlockMaterialType> map = [] {
            std::unordered_map<std::string, BlockMaterialType> out;
            for (const Entry &entry: ENTRIES)
                out.emplace(entry.mIdentifier, entry.mType);
            return out;
        }();
        return map;
    }
}

bool BlockMaterial::find(const BlockState &state, BlockMaterialType &type) {
    const auto found = types().find(state.mName);
    if (found == types().end())
        return false;

    type = found->second;
    return true;
}

uint8_t BlockMaterial::flagsOf(BlockMaterialType type) {
    return FLAGS[static_cast<size_t>(type)];
}

uint8_t BlockMaterial::flagsOf(const BlockState &state) {
    BlockMaterialType type;
    return find(state, type) ? flagsOf(type) : 0;
}
