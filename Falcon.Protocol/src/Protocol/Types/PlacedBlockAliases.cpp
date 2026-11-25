#include "Protocol/Types/PlacedBlockAliases.h"

#include <unordered_map>

namespace PlacedBlockAliases {

namespace {
    const std::unordered_map<std::string, std::string> &itemToBlock() {
        static const std::unordered_map<std::string, std::string> aliases = {
                {"minecraft:redstone",      "minecraft:redstone_wire"},
                {"minecraft:repeater",      "minecraft:unpowered_repeater"},
                {"minecraft:comparator",    "minecraft:unpowered_comparator"},
                {"minecraft:sugar_cane",    "minecraft:reeds"},
                {"minecraft:banner",        "minecraft:standing_banner"},
                {"minecraft:shulker_box",   "minecraft:undyed_shulker_box"},
                {"minecraft:light_block",   "minecraft:light_block_0"},
                {"minecraft:oak_sign",      "minecraft:standing_sign"},
                {"minecraft:birch_sign",    "minecraft:birch_standing_sign"},
                {"minecraft:spruce_sign",   "minecraft:spruce_standing_sign"},
                {"minecraft:acacia_sign",   "minecraft:acacia_standing_sign"},
                {"minecraft:dark_oak_sign", "minecraft:darkoak_standing_sign"},
                {"minecraft:jungle_sign",   "minecraft:jungle_standing_sign"},
                {"minecraft:crimson_sign",  "minecraft:crimson_standing_sign"},
                {"minecraft:warped_sign",   "minecraft:warped_standing_sign"},
                {"minecraft:mangrove_sign", "minecraft:mangrove_standing_sign"},
                {"minecraft:cherry_sign",   "minecraft:cherry_standing_sign"},
                {"minecraft:bamboo_sign",   "minecraft:bamboo_standing_sign"},
                {"minecraft:pale_oak_sign", "minecraft:pale_oak_standing_sign"}
        };

        return aliases;
    }

    const std::string &emptyIdentifier() {
        static const std::string none;
        return none;
    }
}

const std::string &resolve(const std::string &itemIdentifier) {
    const auto &aliases = itemToBlock();
    const auto it = aliases.find(itemIdentifier);
    return it == aliases.end() ? emptyIdentifier() : it->second;
}

const std::string &resolveItem(const std::string &blockIdentifier) {
    static const std::unordered_map<std::string, std::string> blockToItem = [] {
        std::unordered_map<std::string, std::string> reversed;
        for (const auto &entry: itemToBlock())
            reversed[entry.second] = entry.first;

        reversed["minecraft:powered_repeater"] = "minecraft:repeater";
        reversed["minecraft:powered_comparator"] = "minecraft:comparator";
        return reversed;
    }();

    const auto it = blockToItem.find(blockIdentifier);
    return it == blockToItem.end() ? emptyIdentifier() : it->second;
}

}
