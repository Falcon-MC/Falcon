#include "World/Level/Block/Systems/CircuitComponents.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace {
    enum class ComponentKind { None, Source, Wire, Consumer };

    // a block setting up its own component (block vtable slot 56) or a producer hook (slot 115, sub_14331D5D0)
    const std::unordered_set<std::string> &sources() {
        static const std::unordered_set<std::string> names = {
                "minecraft:redstone_torch",
                "minecraft:unlit_redstone_torch",
                "minecraft:powered_repeater",
                "minecraft:unpowered_repeater",
                "minecraft:powered_comparator",
                "minecraft:unpowered_comparator",
                "minecraft:lever",
                "minecraft:redstone_block",
                "minecraft:observer",
                "minecraft:target",
                "minecraft:daylight_detector",
                "minecraft:daylight_detector_inverted",
                "minecraft:tripwire_hook",
                "minecraft:detector_rail",
                "minecraft:sculk_sensor",
                "minecraft:calibrated_sculk_sensor",
                // sub_14A2FDAE0 only for the trapped chest type
                "minecraft:trapped_chest",
                "minecraft:jukebox",
                "minecraft:lectern",
        };
        return names;
    }

    // consumer descriptions (vtable 0x14F187140 in sub_14CE9AD30), consumer hooks (sub_1449BAFC0) and
    // the rail and piston components
    const std::unordered_set<std::string> &consumers() {
        static const std::unordered_set<std::string> names = {
                "minecraft:activator_rail",
                "minecraft:golden_rail",
                "minecraft:piston",
                "minecraft:sticky_piston",
                "minecraft:dispenser",
                "minecraft:dropper",
                "minecraft:noteblock",
                "minecraft:tnt",
                "minecraft:underwater_tnt",
                "minecraft:redstone_lamp",
                "minecraft:lit_redstone_lamp",
                "minecraft:command_block",
                "minecraft:repeating_command_block",
                "minecraft:chain_command_block",
                "minecraft:bell",
                "minecraft:big_dripleaf",
                "minecraft:sculk_shrieker",
                "minecraft:crafter",
                "minecraft:skeleton_skull",
                "minecraft:wither_skeleton_skull",
                "minecraft:zombie_head",
                "minecraft:player_head",
                "minecraft:creeper_head",
                "minecraft:dragon_head",
                "minecraft:piglin_head",
                "minecraft:hopper",
                "minecraft:structure_block",
        };
        return names;
    }

    bool endsWith(const std::string &name, const char *suffix) {
        return BlockIdentifier::endsWith(name, suffix);
    }

    ComponentKind kindOf(const std::string &name) {
        if (name == "minecraft:redstone_wire")
            return ComponentKind::Wire;
        // the button, plate and lightning rod classes, every variant registered with them
        if (sources().count(name) != 0 || endsWith(name, "_button") || endsWith(name, "_pressure_plate")
            || endsWith(name, "lightning_rod"))
            return ComponentKind::Source;
        // the copper bulb and shelf descriptions, the door, trapdoor, fence gate and shulker box hooks
        if (consumers().count(name) != 0 || endsWith(name, "copper_bulb") || endsWith(name, "_shelf")
            || endsWith(name, "door") || endsWith(name, "fence_gate") || endsWith(name, "shulker_box"))
            return ComponentKind::Consumer;
        // a pack block with minecraft:redstone_consumer (sub_143879850)
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(name);
        if (custom != nullptr && custom->mRedstoneConsumer)
            return ComponentKind::Consumer;
        return ComponentKind::None;
    }
}

std::optional<int> CircuitComponents::strengthAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    switch (kindOf(state.mName)) {
        case ComponentKind::Wire:
            return std::clamp(state.mStates.getInt("redstone_signal", 0), 0, RedstoneSystem::MAX_SIGNAL);
        case ComponentKind::Source: {
            // a producer or capacitor holds the signal it sends out
            int strength = 0;
            for (int face = 0; face < RedstoneFace::COUNT; ++face)
                strength = std::max(strength, RedstoneSystem::getWeakPower(owner, level, position, face));
            return strength;
        }
        case ComponentKind::Consumer:
            // a consumer holds the strongest signal it receives
            return RedstoneSystem::isBlockIndirectlyGettingPowered(owner, level, position);
        default:
            return std::nullopt;
    }
}
