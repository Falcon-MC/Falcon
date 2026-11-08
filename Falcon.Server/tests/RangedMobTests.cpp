#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Component/MobShooter.h"

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool closeTo(float value, float expected, float tolerance) {
        return std::fabs(value - expected) <= tolerance;
    }

    const json::Value &path(const json::Value *root, std::initializer_list<const char *> keys) {
        const json::Value *value = root;
        for (const char *key: keys) {
            require(value != nullptr, key);
            value = value->get(key);
        }
        require(value != nullptr, "missing definition field");
        return *value;
    }

    void aimPitch() {
        require(MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.0f) == 0.0f, "no gravity aims straight");
        // mob_arrow: power 1.6, gravity 0.05, level 10 blocks away
        require(closeTo(MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.05f), -5.63f, 0.02f), "low arc at 10 blocks");
        require(MobShooter::aimPitch(10.0f, 2.0f, 1.6f, 0.05f) < MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.05f),
                "a higher target needs a steeper arc");
        require(MobShooter::aimPitch(100.0f, 0.0f, 1.6f, 0.05f) == -45.0f, "out of reach shoots 45 degrees up");
    }

    void uncertainty() {
        // mob_arrow uncertainty_base 16, uncertainty_multiplier 4
        require(MobShooter::uncertainty(16.0f, 4.0f, 0) == 16.0f, "peaceful");
        require(MobShooter::uncertainty(16.0f, 4.0f, 1) == 12.0f, "easy");
        require(MobShooter::uncertainty(16.0f, 4.0f, 2) == 8.0f, "normal");
        require(MobShooter::uncertainty(16.0f, 4.0f, 3) == 4.0f, "hard");
        require(MobShooter::uncertainty(1.0f, 4.0f, 3) == 0.0f, "never negative");
    }

    void vanillaDefinitions() {
        EntityDefinitions::initialize();

        const json::Value *arrow = EntityDefinitions::find("minecraft:arrow");
        const json::Value &mobArrow = path(arrow, {"component_groups", "minecraft:mob_arrow", "minecraft:projectile"});
        require(closeTo((float) mobArrow.get("power")->number(), 1.6f, 0.0001f), "mob arrow power");
        require(mobArrow.get("uncertainty_base")->integer() == 16, "mob arrow uncertainty base");

        const json::Value &stray = path(EntityDefinitions::find("minecraft:stray"),
                                        {"components", "minecraft:shooter", "aux_val"});
        require(stray.integer() == 19, "stray shoots slowness arrows");

        const json::Value &bogged = path(EntityDefinitions::find("minecraft:bogged"),
                                         {"component_groups", "minecraft:ranged_attack", "minecraft:shooter",
                                          "aux_val"});
        require(bogged.integer() == 26, "bogged shoots poison arrows");

        const json::Value &pillager = path(EntityDefinitions::find("minecraft:pillager"),
                                           {"component_groups", "minecraft:ranged_attack",
                                            "minecraft:behavior.ranged_attack", "attack_radius"});
        require(pillager.integer() == 8, "pillager attack radius");
    }
}

int main() {
    try {
        aimPitch();
        uncertainty();
        vanillaDefinitions();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "ranged mob tests passed" << std::endl;
    return 0;
}
