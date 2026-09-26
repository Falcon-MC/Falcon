#include "Item/PotionEffects.h"

namespace {
    PotionEffect timed(MobEffectId id, int32_t duration, int32_t amplifier) {
        return PotionEffect{id, duration, amplifier, false};
    }

    PotionEffect instant(MobEffectId id, int32_t amplifier) {
        return PotionEffect{id, 0, amplifier, true};
    }

    const char *const POTION_NAMES[] = {
            "water", "mundane", "long_mundane", "thick", "awkward", "nightvision", "long_nightvision", "invisibility",
            "long_invisibility", "leaping", "long_leaping", "strong_leaping", "fire_resistance",
            "long_fire_resistance", "swiftness", "long_swiftness", "strong_swiftness", "slowness", "long_slowness",
            "water_breathing", "long_water_breathing", "healing", "strong_healing", "harming", "strong_harming",
            "poison", "long_poison", "strong_poison", "regeneration", "long_regeneration", "strong_regeneration",
            "strength", "long_strength", "strong_strength", "weakness", "long_weakness", "wither", "turtle_master",
            "long_turtle_master", "strong_turtle_master", "slow_falling", "long_slow_falling", "strong_slowness",
            "wind_charged", "weaving", "oozing", "infested"
    };
}

int32_t findPotionId(const std::string &name) {
    const std::string key = name.rfind("minecraft:", 0) == 0 ? name.substr(10) : name;
    const int32_t count = (int32_t) (sizeof(POTION_NAMES) / sizeof(POTION_NAMES[0]));
    for (int32_t id = 0; id < count; ++id) {
        if (key == POTION_NAMES[id])
            return id;
    }
    return -1;
}

int32_t getPotionColor(int32_t potionId) {
    const std::vector<PotionEffect> effects = getPotionEffects(potionId);
    if (effects.empty())
        return (int32_t) 0xFF385DC6;

    switch (effects.front().mId) {
        case MobEffectId::Speed:
            return (int32_t) 0xFF7CAFC6;
        case MobEffectId::Slowness:
            return (int32_t) 0xFF5A6C81;
        case MobEffectId::Strength:
            return (int32_t) 0xFF932423;
        case MobEffectId::InstantHealth:
            return (int32_t) 0xFFF82423;
        case MobEffectId::InstantDamage:
            return (int32_t) 0xFF430A09;
        case MobEffectId::JumpBoost:
            return (int32_t) 0xFF22FF4C;
        case MobEffectId::Regeneration:
            return (int32_t) 0xFFCD5CAB;
        case MobEffectId::FireResistance:
            return (int32_t) 0xFFE49A3A;
        case MobEffectId::WaterBreathing:
            return (int32_t) 0xFF2E5299;
        case MobEffectId::Invisibility:
            return (int32_t) 0xFF7F8392;
        case MobEffectId::NightVision:
            return (int32_t) 0xFF1F1FA1;
        case MobEffectId::Weakness:
            return (int32_t) 0xFF484D48;
        case MobEffectId::Poison:
            return (int32_t) 0xFF4E9331;
        case MobEffectId::Wither:
            return (int32_t) 0xFF352A27;
        case MobEffectId::SlowFalling:
            return (int32_t) 0xFFF7F8E0;
        default:
            return (int32_t) 0xFF385DC6;
    }
}

std::vector<PotionEffect> getPotionEffects(int32_t potionId) {
    switch (potionId) {
        case 5:
            return {timed(MobEffectId::NightVision, 3600, 0)};
        case 6:
            return {timed(MobEffectId::NightVision, 9600, 0)};
        case 7:
            return {timed(MobEffectId::Invisibility, 3600, 0)};
        case 8:
            return {timed(MobEffectId::Invisibility, 9600, 0)};
        case 9:
            return {timed(MobEffectId::JumpBoost, 3600, 0)};
        case 10:
            return {timed(MobEffectId::JumpBoost, 9600, 0)};
        case 11:
            return {timed(MobEffectId::JumpBoost, 1800, 1)};
        case 12:
            return {timed(MobEffectId::FireResistance, 3600, 0)};
        case 13:
            return {timed(MobEffectId::FireResistance, 9600, 0)};
        case 14:
            return {timed(MobEffectId::Speed, 3600, 0)};
        case 15:
            return {timed(MobEffectId::Speed, 9600, 0)};
        case 16:
            return {timed(MobEffectId::Speed, 1800, 1)};
        case 17:
            return {timed(MobEffectId::Slowness, 1800, 0)};
        case 18:
            return {timed(MobEffectId::Slowness, 4800, 0)};
        case 19:
            return {timed(MobEffectId::WaterBreathing, 3600, 0)};
        case 20:
            return {timed(MobEffectId::WaterBreathing, 9600, 0)};
        case 21:
            return {instant(MobEffectId::InstantHealth, 0)};
        case 22:
            return {instant(MobEffectId::InstantHealth, 1)};
        case 23:
            return {instant(MobEffectId::InstantDamage, 0)};
        case 24:
            return {instant(MobEffectId::InstantDamage, 1)};
        case 25:
            return {timed(MobEffectId::Poison, 900, 0)};
        case 26:
            return {timed(MobEffectId::Poison, 1800, 0)};
        case 27:
            return {timed(MobEffectId::Poison, 432, 1)};
        case 28:
            return {timed(MobEffectId::Regeneration, 900, 0)};
        case 29:
            return {timed(MobEffectId::Regeneration, 1800, 0)};
        case 30:
            return {timed(MobEffectId::Regeneration, 450, 1)};
        case 31:
            return {timed(MobEffectId::Strength, 3600, 0)};
        case 32:
            return {timed(MobEffectId::Strength, 9600, 0)};
        case 33:
            return {timed(MobEffectId::Strength, 1800, 1)};
        case 34:
            return {timed(MobEffectId::Weakness, 1800, 0)};
        case 35:
            return {timed(MobEffectId::Weakness, 4800, 0)};
        case 36:
            return {timed(MobEffectId::Wither, 800, 0)};
        case 37:
            return {timed(MobEffectId::Slowness, 400, 3), timed(MobEffectId::Resistance, 400, 2)};
        case 38:
            return {timed(MobEffectId::Slowness, 800, 5), timed(MobEffectId::Resistance, 800, 2)};
        case 39:
            return {timed(MobEffectId::Slowness, 400, 5), timed(MobEffectId::Resistance, 400, 3)};
        case 40:
            return {timed(MobEffectId::SlowFalling, 1800, 0)};
        case 41:
            return {timed(MobEffectId::SlowFalling, 4800, 0)};
        default:
            return {};
    }
}
