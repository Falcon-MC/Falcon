#include "Block/Components/BlockBehaviorRegistry.h"

#include "Block/Components/BlockBehavior.h"
#include "Block/Blocks/BlueIceBlockBehavior.h"
#include "Block/Blocks/IceBlockBehavior.h"
#include "Block/Blocks/SlimeBlockBehavior.h"

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

    void registerVanillaBehaviors() {
        static const bool registered = [] {
            BlockBehaviorRegistry::registerBehavior("minecraft:slime", slimeBehavior());
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
