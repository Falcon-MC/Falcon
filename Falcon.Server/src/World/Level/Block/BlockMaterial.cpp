#include "World/Level/Block/BlockMaterial.h"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace {
    struct Entry {
        const char *mIdentifier;
        BlockMaterialType mType;
        bool mProperty;
    };

    // FLAGS: the game's (type, flags) pairs, indexed by type
#include "BlockMaterialTable.inc"

    static_assert(sizeof(FLAGS) == static_cast<size_t>(BlockMaterialType::NonSolid) + 1, "one flag byte per type");

    const Entry *entryOf(const BlockState &state) {
        static const std::unordered_map<std::string, const Entry *> map = [] {
            std::unordered_map<std::string, const Entry *> out;
            for (const Entry &entry: ENTRIES)
                out.emplace(entry.mIdentifier, &entry);
            return out;
        }();
        const auto found = map.find(state.mName);
        return found == map.end() ? nullptr : found->second;
    }
}

bool BlockMaterial::find(const BlockState &state, BlockMaterialType &type) {
    const Entry *entry = entryOf(state);
    if (entry == nullptr)
        return false;

    type = entry->mType;
    return true;
}

bool BlockMaterial::hasDefaultProperty(const BlockState &state) {
    const Entry *entry = entryOf(state);
    return entry != nullptr && entry->mProperty;
}

uint8_t BlockMaterial::flagsOf(BlockMaterialType type) {
    return FLAGS[static_cast<size_t>(type)];
}

uint8_t BlockMaterial::flagsOf(const BlockState &state) {
    BlockMaterialType type;
    return find(state, type) ? flagsOf(type) : 0;
}
