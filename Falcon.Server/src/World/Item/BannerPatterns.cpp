#include "World/Item/BannerPatterns.h"

#include "Core/Color/DyeColor.h"

#include <cstring>
#include <utility>
#include <vector>

namespace {
    const char *const BANNER = "minecraft:banner";
    const char *const TAG_PATTERNS = "Patterns";
    const char *const TAG_PATTERN = "Pattern";
    const char *const TAG_COLOR = "Color";
    const char *const TAG_TYPE = "Type";

    using BannerPatterns::NO_ITEM;
    using BannerPatterns::Pattern;

    // the game's registry, in its order
    const Pattern PATTERNS[] = {
            {"base", "", NO_ITEM, true},
            {"border", "bo", NO_ITEM, false},
            {"bricks", "bri", 4, false},
            {"circle", "mc", NO_ITEM, false},
            {"creeper", "cre", 0, false},
            {"skull", "sku", 1, false},
            {"cross", "cr", NO_ITEM, false},
            {"curly_border", "cbo", 5, false},
            {"diagonal_up_left", "ld", NO_ITEM, false},
            {"diagonal_up_right", "rud", NO_ITEM, false},
            {"diagonal_left", "lud", NO_ITEM, false},
            {"diagonal_right", "rd", NO_ITEM, false},
            {"flower", "flo", 2, false},
            {"gradient", "gra", NO_ITEM, false},
            {"gradient_up", "gru", NO_ITEM, false},
            {"half_horizontal", "hh", NO_ITEM, false},
            {"half_horizontal_bottom", "hhb", NO_ITEM, false},
            {"half_vertical", "vh", NO_ITEM, false},
            {"half_vertical_right", "vhr", NO_ITEM, false},
            {"illager", "ill", NO_ITEM, true},
            {"mojang", "moj", 3, false},
            {"rhombus", "mr", NO_ITEM, false},
            {"small_stripes", "ss", NO_ITEM, false},
            {"square_bottom_left", "bl", NO_ITEM, false},
            {"square_bottom_right", "br", NO_ITEM, false},
            {"square_top_left", "tl", NO_ITEM, false},
            {"square_top_right", "tr", NO_ITEM, false},
            {"straight_cross", "sc", NO_ITEM, false},
            {"stripe_bottom", "bs", NO_ITEM, false},
            {"stripe_center", "cs", NO_ITEM, false},
            {"stripe_downleft", "dls", NO_ITEM, false},
            {"stripe_downright", "drs", NO_ITEM, false},
            {"stripe_left", "ls", NO_ITEM, false},
            {"stripe_middle", "ms", NO_ITEM, false},
            {"stripe_right", "rs", NO_ITEM, false},
            {"stripe_top", "ts", NO_ITEM, false},
            {"triangle_bottom", "bt", NO_ITEM, false},
            {"triangle_top", "tt", NO_ITEM, false},
            {"triangles_bottom", "bts", NO_ITEM, false},
            {"triangles_top", "tts", NO_ITEM, false},
            {"piglin", "pig", 6, false},
            {"globe", "glb", 7, false},
            {"flow", "flw", 8, false},
            {"guster", "gus", 9, false},
    };

    // the banner pattern items by the index their patterns ask for
    const char *const PATTERN_ITEMS[] = {
            "minecraft:creeper_banner_pattern",       "minecraft:skull_banner_pattern",
            "minecraft:flower_banner_pattern",        "minecraft:mojang_banner_pattern",
            "minecraft:field_masoned_banner_pattern", "minecraft:bordure_indented_banner_pattern",
            "minecraft:piglin_banner_pattern",        "minecraft:globe_banner_pattern",
            "minecraft:flow_banner_pattern",          "minecraft:guster_banner_pattern",
    };

    const std::string &identifierOf(const ItemStack &item) {
        static const std::string none;
        return item.isAir() ? none : item.mDefinition->getIdentifier();
    }
}

const Pattern *BannerPatterns::find(const std::string &id) {
    for (const Pattern &pattern: PATTERNS) {
        if (id == pattern.mId)
            return &pattern;
    }
    return nullptr;
}

int16_t BannerPatterns::itemIndexOf(const ItemStack &item) {
    const std::string &identifier = identifierOf(item);
    for (size_t index = 0; index < sizeof(PATTERN_ITEMS) / sizeof(PATTERN_ITEMS[0]); ++index) {
        if (identifier == PATTERN_ITEMS[index])
            return (int16_t) index;
    }
    return NO_ITEM;
}

// only the sixteen dyes: ink sacs, lapis, cocoa beans and bone meal are crafted into dyes first
uint8_t BannerPatterns::dyeColorOf(const ItemStack &item) {
    const uint8_t color = DyeColor::fromIdentifier(identifierOf(item), "_dye");
    return color == DyeColor::INVALID ? DyeColor::INVALID : DyeColor::invert(color);
}

// the ominous banner (Type 1) takes no more layers
bool BannerPatterns::isBanner(const ItemStack &item) {
    if (identifierOf(item) != BANNER)
        return false;
    const Tag *type = item.mTag.isCompound() ? item.mTag.get(TAG_TYPE) : nullptr;
    return type == nullptr || type->getType() != Tag::Type::Int || type->asInt() == 0;
}

int32_t BannerPatterns::patternCount(const ItemStack &banner) {
    const Tag *patterns = banner.mTag.isCompound() ? banner.mTag.get(TAG_PATTERNS) : nullptr;
    return patterns != nullptr && patterns->isList() ? (int32_t) patterns->getList().size() : 0;
}

ItemStack BannerPatterns::loomResult(const ItemStack &banner, const ItemStack &dye, const ItemStack &material,
                                     const Pattern &pattern) {
    if (pattern.mHidden || !isBanner(banner) || patternCount(banner) >= MAX_LOOM_PATTERNS)
        return ItemStack::air();

    const uint8_t color = dyeColorOf(dye);
    if (color == DyeColor::INVALID)
        return ItemStack::air();

    // a dye-only pattern is refused while a pattern item sits in the material slot
    if (pattern.mItemIndex == NO_ITEM ? !material.isAir() : itemIndexOf(material) != pattern.mItemIndex)
        return ItemStack::air();

    ItemStack result = banner;
    result.mCount = 1;
    result.mUsingNetId = false;
    result.mNetId = 0;
    if (!result.mTag.isCompound())
        result.mTag = Tag::ofCompound();

    // the old layers are copied as Pattern and Color only, in their order
    std::vector<Tag> layers;
    const Tag *patterns = result.mTag.get(TAG_PATTERNS);
    if (patterns != nullptr && patterns->isList()) {
        for (const Tag &layer: patterns->getList()) {
            if (!layer.isCompound() || layer.get(TAG_PATTERN) == nullptr || layer.get(TAG_COLOR) == nullptr)
                continue;
            Tag kept = Tag::ofCompound();
            kept.putString(TAG_PATTERN, layer.getString(TAG_PATTERN));
            kept.putInt(TAG_COLOR, layer.getInt(TAG_COLOR, 0));
            layers.push_back(std::move(kept));
        }
    }

    Tag added = Tag::ofCompound();
    added.putString(TAG_PATTERN, pattern.mId);
    added.putInt(TAG_COLOR, color);
    layers.push_back(std::move(added));
    result.mTag.put(TAG_PATTERNS, Tag::ofList(Tag::Type::Compound, std::move(layers)));
    return result;
}
