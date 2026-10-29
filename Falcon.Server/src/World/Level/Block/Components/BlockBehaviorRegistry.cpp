#include "World/Level/Block/Components/BlockBehaviorRegistry.h"

#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Level/Block/BedBlockBehavior.h"
#include "World/Level/Block/BlueIceBlockBehavior.h"
#include "World/Level/Block/HoneyBlockBehavior.h"
#include "World/Level/Block/IceBlockBehavior.h"
#include "World/Level/Block/SlimeBlockBehavior.h"
#include "World/Level/Block/SoulSpeedBlockBehavior.h"

#include <unordered_map>

namespace {
    using BehaviorMap = std::unordered_map<std::string, const BlockBehavior *>;

    BehaviorMap &behaviorMap() {
        static BehaviorMap map;
        return map;
    }

    const BlockBehavior &defaultBehavior() {
        static const BlockBehavior behavior;
        return behavior;
    }

    const SlimeBlockBehavior &slimeBehavior() {
        static const SlimeBlockBehavior behavior;
        return behavior;
    }

    const IceBlockBehavior &iceBehavior() {
        static const IceBlockBehavior behavior;
        return behavior;
    }

    const BlueIceBlockBehavior &blueIceBehavior() {
        static const BlueIceBlockBehavior behavior;
        return behavior;
    }

    const BedBlockBehavior &bedBehavior() {
        static const BedBlockBehavior behavior;
        return behavior;
    }

    const HoneyBlockBehavior &honeyBehavior() {
        static const HoneyBlockBehavior behavior;
        return behavior;
    }

    void registerVanillaBehaviors() {
        static const bool registered = [] {
            static const SoulSpeedBlockBehavior soulSpeed;
            BlockBehaviorRegistry::registerBehavior("minecraft:soul_sand", soulSpeed);
            BlockBehaviorRegistry::registerBehavior("minecraft:soul_soil", soulSpeed);
            BlockBehaviorRegistry::registerBehavior("minecraft:slime", slimeBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:bed", bedBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:honey_block", honeyBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:ice", iceBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:packed_ice", iceBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:frosted_ice", iceBehavior());
            BlockBehaviorRegistry::registerBehavior("minecraft:blue_ice", blueIceBehavior());
            return true;
        }();
        (void) registered;
    }
}

const BlockBehavior &BlockBehaviorRegistry::get(const std::string &identifier) {
    registerVanillaBehaviors();

    const auto it = behaviorMap().find(identifier);
    return it == behaviorMap().end() ? defaultBehavior() : *it->second;
}

void BlockBehaviorRegistry::registerBehavior(const std::string &identifier, const BlockBehavior &behavior) {
    behaviorMap()[identifier] = &behavior;
}
