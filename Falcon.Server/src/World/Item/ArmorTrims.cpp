#include "World/Item/ArmorTrims.h"

namespace {
    struct PatternEntry {
        const char *mItemName;
        const char *mPatternId;
    };

    struct MaterialEntry {
        const char *mMaterialId;
        const char *mColor;
        const char *mItemName;
    };

    // Registration order of the 1.26.52 trim registries.
    const PatternEntry PATTERNS[] = {
            {"minecraft:ward_armor_trim_smithing_template",      "ward"},
            {"minecraft:snout_armor_trim_smithing_template",     "snout"},
            {"minecraft:dune_armor_trim_smithing_template",      "dune"},
            {"minecraft:spire_armor_trim_smithing_template",     "spire"},
            {"minecraft:wild_armor_trim_smithing_template",      "wild"},
            {"minecraft:rib_armor_trim_smithing_template",       "rib"},
            {"minecraft:tide_armor_trim_smithing_template",      "tide"},
            {"minecraft:sentry_armor_trim_smithing_template",    "sentry"},
            {"minecraft:coast_armor_trim_smithing_template",     "coast"},
            {"minecraft:eye_armor_trim_smithing_template",       "eye"},
            {"minecraft:vex_armor_trim_smithing_template",       "vex"},
            {"minecraft:silence_armor_trim_smithing_template",   "silence"},
            {"minecraft:wayfinder_armor_trim_smithing_template", "wayfinder"},
            {"minecraft:raiser_armor_trim_smithing_template",    "raiser"},
            {"minecraft:shaper_armor_trim_smithing_template",    "shaper"},
            {"minecraft:host_armor_trim_smithing_template",      "host"},
            {"minecraft:flow_armor_trim_smithing_template",      "flow"},
            {"minecraft:bolt_armor_trim_smithing_template",      "bolt"}
    };

    const MaterialEntry MATERIALS[] = {
            {"quartz",    "\xC2\xA7h", "minecraft:quartz"},
            {"iron",      "\xC2\xA7i", "minecraft:iron_ingot"},
            {"netherite", "\xC2\xA7j", "minecraft:netherite_ingot"},
            {"redstone",  "\xC2\xA7m", "minecraft:redstone"},
            {"copper",    "\xC2\xA7n", "minecraft:copper_ingot"},
            {"gold",      "\xC2\xA7p", "minecraft:gold_ingot"},
            {"emerald",   "\xC2\xA7q", "minecraft:emerald"},
            {"diamond",   "\xC2\xA7s", "minecraft:diamond"},
            {"lapis",     "\xC2\xA7t", "minecraft:lapis_lazuli"},
            {"amethyst",  "\xC2\xA7u", "minecraft:amethyst_shard"},
            {"resin",     "\xC2\xA7v", "minecraft:resin_brick"}
    };
}

TrimDataPacket ArmorTrims::getPacket() {
    TrimDataPacket packet;
    for (const PatternEntry &entry: PATTERNS) {
        TrimPattern pattern;
        pattern.mItemName = entry.mItemName;
        pattern.mPatternId = entry.mPatternId;
        packet.mPatterns.push_back(pattern);
    }

    for (const MaterialEntry &entry: MATERIALS) {
        TrimMaterial material;
        material.mMaterialId = entry.mMaterialId;
        material.mColor = entry.mColor;
        material.mItemName = entry.mItemName;
        packet.mMaterials.push_back(material);
    }
    return packet;
}
