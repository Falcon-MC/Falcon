#include "Block/BlockPickItem.h"

#include "Block/BlockActor.h"
#include "Block/BlockIdentifier.h"
#include "Protocol/Types/ItemStack.h"

#include <string_view>
#include <unordered_map>

namespace {
    const char *BLOCK_ENTITY_TAG = "BlockEntityTag";
    const char *DATA_LORE = "+(DATA)";

    bool isBlockLink(std::string_view key) {
        return BlockIdentifier::equalsAny(key, {"pairx", "pairz", "pairlead"});
    }

    const std::unordered_map<std::string_view, std::string_view> &renamedItems() {
        static const std::unordered_map<std::string_view, std::string_view> ITEMS = {
                {"minecraft:lit_furnace", "minecraft:furnace"},
                {"minecraft:lit_blast_furnace", "minecraft:blast_furnace"},
                {"minecraft:lit_smoker", "minecraft:smoker"},
                {"minecraft:lit_redstone_lamp", "minecraft:redstone_lamp"},
                {"minecraft:lit_redstone_ore", "minecraft:redstone_ore"},
                {"minecraft:lit_deepslate_redstone_ore", "minecraft:deepslate_redstone_ore"},
                {"minecraft:unlit_redstone_torch", "minecraft:redstone_torch"},
                {"minecraft:powered_repeater", "minecraft:repeater"},
                {"minecraft:unpowered_repeater", "minecraft:repeater"},
                {"minecraft:powered_comparator", "minecraft:comparator"},
                {"minecraft:unpowered_comparator", "minecraft:comparator"},
                {"minecraft:daylight_detector_inverted", "minecraft:daylight_detector"},
                {"minecraft:redstone_wire", "minecraft:redstone"},
                {"minecraft:trip_wire", "minecraft:string"},
                {"minecraft:reeds", "minecraft:sugar_cane"},
                {"minecraft:wheat", "minecraft:wheat_seeds"},
                {"minecraft:beetroot", "minecraft:beetroot_seeds"},
                {"minecraft:carrots", "minecraft:carrot"},
                {"minecraft:potatoes", "minecraft:potato"},
                {"minecraft:pumpkin_stem", "minecraft:pumpkin_seeds"},
                {"minecraft:melon_stem", "minecraft:melon_seeds"},
                {"minecraft:torchflower_crop", "minecraft:torchflower_seeds"},
                {"minecraft:pitcher_crop", "minecraft:pitcher_pod"},
                {"minecraft:cocoa", "minecraft:cocoa_beans"},
                {"minecraft:sweet_berry_bush", "minecraft:sweet_berries"},
                {"minecraft:cave_vines", "minecraft:glow_berries"},
                {"minecraft:cave_vines_body_with_berries", "minecraft:glow_berries"},
                {"minecraft:cave_vines_head_with_berries", "minecraft:glow_berries"},
                {"minecraft:bamboo_sapling", "minecraft:bamboo"},
                {"minecraft:farmland", "minecraft:dirt"},
                {"minecraft:standing_banner", "minecraft:banner"},
                {"minecraft:wall_banner", "minecraft:banner"},
                {"minecraft:standing_sign", "minecraft:oak_sign"},
                {"minecraft:wall_sign", "minecraft:oak_sign"},
                {"minecraft:darkoak_standing_sign", "minecraft:dark_oak_sign"},
                {"minecraft:darkoak_wall_sign", "minecraft:dark_oak_sign"}
        };
        return ITEMS;
    }

    bool isUnpickable(std::string_view identifier) {
        return BlockIdentifier::equalsAny(identifier, {
                "minecraft:air", "minecraft:fire", "minecraft:soul_fire", "minecraft:portal",
                "minecraft:end_portal", "minecraft:end_gateway", "minecraft:bubble_column",
                "minecraft:frosted_ice", "minecraft:invisible_bedrock", "minecraft:moving_block",
                "minecraft:piston_arm_collision", "minecraft:sticky_piston_arm_collision",
                "minecraft:water", "minecraft:flowing_water", "minecraft:lava", "minecraft:flowing_lava"
        });
    }

    std::string replaceSuffix(const std::string &identifier, std::string_view suffix, std::string_view replacement) {
        return identifier.substr(0, identifier.size() - suffix.size()) + std::string(replacement);
    }
}

std::string BlockPickItem::identifierFor(const std::string &blockIdentifier) {
    if (isUnpickable(blockIdentifier))
        return std::string();

    const auto renamed = renamedItems().find(blockIdentifier);
    if (renamed != renamedItems().end())
        return std::string(renamed->second);

    if (BlockIdentifier::endsWith(blockIdentifier, "_standing_sign"))
        return replaceSuffix(blockIdentifier, "_standing_sign", "_sign");

    if (BlockIdentifier::endsWith(blockIdentifier, "_wall_sign"))
        return replaceSuffix(blockIdentifier, "_wall_sign", "_sign");

    if (BlockIdentifier::endsWith(blockIdentifier, "_double_slab"))
        return replaceSuffix(blockIdentifier, "_double_slab", "_slab");

    if (BlockIdentifier::endsWith(blockIdentifier, "_wall_fan"))
        return replaceSuffix(blockIdentifier, "_wall_fan", "_fan");

    return blockIdentifier;
}

void BlockPickItem::attachBlockData(ItemStack &item, const BlockActor &actor) {
    const Tag saved = actor.saveNbt();
    if (!saved.isCompound())
        return;

    Tag data = Tag::ofCompound();
    const std::vector<std::string> &keys = saved.getKeys();
    const std::vector<Tag> &values = saved.getValues();
    for (size_t index = 0; index < keys.size(); ++index) {
        if (!isBlockLink(keys[index]))
            data.put(keys[index], values[index]);
    }

    if (!item.mTag.isCompound())
        item.mTag = Tag::ofCompound();

    item.mTag.put(BLOCK_ENTITY_TAG, std::move(data));

    Tag display = Tag::ofCompound();
    display.put("Lore", Tag::ofList(Tag::Type::String, {Tag::ofString(DATA_LORE)}));
    item.mTag.put("display", std::move(display));
}

void BlockPickItem::restoreBlockData(BlockActor &actor, const ItemStack &item, const PacketCodecContext &context) {
    if (!item.mTag.isCompound())
        return;

    const Tag *stored = item.mTag.get(BLOCK_ENTITY_TAG);
    if (stored == nullptr || !stored->isCompound())
        return;

    Tag data = actor.saveNbt();
    if (!data.isCompound())
        data = Tag::ofCompound();

    const std::vector<std::string> &keys = stored->getKeys();
    const std::vector<Tag> &values = stored->getValues();
    for (size_t index = 0; index < keys.size(); ++index) {
        if (!isBlockLink(keys[index]))
            data.put(keys[index], values[index]);
    }

    actor.loadNbt(data, context);
}
