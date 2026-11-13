#include "World/Village/PoiBlockTypes.h"

#include <algorithm>
#include <unordered_map>

namespace {
    struct TypeDocument {
        const char *mName;
        uint8_t mTickets;
        uint8_t mUsableRange;
        std::vector<std::string> mBlocks;
        std::vector<std::string> mTags;
    };

    struct TagDocument {
        const char *mName;
        // tags whose types the tag takes in
        std::vector<std::string> mTags;
    };

    const char *const JOB_SITE = "minecraft:acquirable_job_site";
    const char *const BEE_HOME = "minecraft:bee_home";

    // behavior_packs/experimental_poi poi/blocks, in the archive's order: name, tickets, usable range, blocks, tags
    const std::vector<TypeDocument> &typeDocuments() {
        static const std::vector<TypeDocument> documents = {
                {"minecraft:armorer", 1, 1, {"minecraft:blast_furnace"}, {JOB_SITE}},
                {"minecraft:bee_nest", 0, 1, {"minecraft:bee_nest"}, {BEE_HOME}},
                {"minecraft:beehive", 0, 1, {"minecraft:beehive"}, {BEE_HOME}},
                {"minecraft:butcher", 1, 1, {"minecraft:smoker"}, {JOB_SITE}},
                {"minecraft:cartographer", 1, 1, {"minecraft:cartography_table"}, {JOB_SITE}},
                {"minecraft:cleric", 1, 1, {"minecraft:brewing_stand"}, {JOB_SITE}},
                {"minecraft:farmer", 1, 1, {"minecraft:composter"}, {JOB_SITE}},
                {"minecraft:fisherman", 1, 1, {"minecraft:barrel"}, {JOB_SITE}},
                {"minecraft:fletcher", 1, 1, {"minecraft:fletching_table"}, {JOB_SITE}},
                {"minecraft:home", 1, 1, {"minecraft:bed"}, {"minecraft:village"}},
                {"minecraft:leatherworker", 1, 1, {"minecraft:cauldron"}, {JOB_SITE}},
                {"minecraft:librarian", 1, 1, {"minecraft:lectern"}, {JOB_SITE}},
                {"minecraft:lightning_rod",
                 0,
                 1,
                 {"minecraft:lightning_rod", "minecraft:exposed_lightning_rod", "minecraft:oxidized_lightning_rod",
                  "minecraft:weathered_lightning_rod", "minecraft:waxed_lightning_rod",
                  "minecraft:waxed_exposed_lightning_rod", "minecraft:waxed_oxidized_lightning_rod",
                  "minecraft:waxed_weathered_lightning_rod"},
                 {}},
                {"minecraft:lodestone", 0, 1, {"minecraft:lodestone"}, {}},
                {"minecraft:mason", 1, 1, {"minecraft:stonecutter_block"}, {JOB_SITE}},
                {"minecraft:meeting", 32, 6, {"minecraft:bell"}, {"minecraft:village"}},
                {"minecraft:shepherd", 1, 1, {"minecraft:loom"}, {JOB_SITE}},
                {"minecraft:toolsmith", 1, 1, {"minecraft:smithing_table"}, {JOB_SITE}},
                {"minecraft:weaponsmith", 1, 1, {"minecraft:grindstone"}, {JOB_SITE}},
        };
        return documents;
    }

    // poi/tags of the same pack
    const std::vector<TagDocument> &tagDocuments() {
        static const std::vector<TagDocument> documents = {{"minecraft:village", {JOB_SITE}}};
        return documents;
    }

    std::vector<PoiBlockType> build() {
        std::vector<PoiBlockType> types;
        for (const TypeDocument &document: typeDocuments()) {
            PoiBlockType type;
            type.mId = (uint16_t) types.size();
            type.mName = document.mName;
            type.mTickets = document.mTickets;
            type.mUsableRange = document.mUsableRange;
            type.mBlocks = document.mBlocks;
            type.mTags = document.mTags;
            for (const TagDocument &tag: tagDocuments()) {
                const bool included = std::any_of(tag.mTags.begin(), tag.mTags.end(), [&](const std::string &name) {
                    return std::find(document.mTags.begin(), document.mTags.end(), name) != document.mTags.end();
                });
                if (included && std::find(type.mTags.begin(), type.mTags.end(), tag.mName) == type.mTags.end())
                    type.mTags.emplace_back(tag.mName);
            }
            types.push_back(std::move(type));
        }
        return types;
    }

    const std::unordered_map<std::string, uint16_t> &byBlock() {
        static const std::unordered_map<std::string, uint16_t> table = [] {
            std::unordered_map<std::string, uint16_t> blocks;
            for (const PoiBlockType &type: PoiBlockTypes::getAll()) {
                for (const std::string &block: type.mBlocks)
                    blocks.emplace(block, type.mId);
            }
            return blocks;
        }();
        return table;
    }
}

bool PoiBlockType::hasTag(const std::string &tag) const {
    return std::find(mTags.begin(), mTags.end(), tag) != mTags.end();
}

const std::vector<PoiBlockType> &PoiBlockTypes::getAll() {
    static const std::vector<PoiBlockType> types = build();
    return types;
}

const PoiBlockType *PoiBlockTypes::get(uint16_t id) {
    const std::vector<PoiBlockType> &types = getAll();
    return id < types.size() ? &types[id] : nullptr;
}

const PoiBlockType *PoiBlockTypes::find(const std::string &name) {
    for (const PoiBlockType &type: getAll()) {
        if (type.mName == name)
            return &type;
    }
    return nullptr;
}

const PoiBlockType *PoiBlockTypes::forBlock(const std::string &blockName) {
    const auto found = byBlock().find(blockName);
    return found == byBlock().end() ? nullptr : get(found->second);
}
