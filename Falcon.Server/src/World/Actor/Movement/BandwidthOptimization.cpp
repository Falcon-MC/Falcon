#include "World/Actor/Movement/BandwidthOptimization.h"

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"

#include <algorithm>
#include <memory>

namespace {
    const char *const COMPONENT = "minecraft:conditional_bandwidth_optimization";
    // the game keeps three quarters of max_dropped_ticks as the widest gap
    const float GAP_SCALE = 0.75f;

    void readValues(const json::Value &entry, BandwidthOptimization::Values &values) {
        BandwidthOptimization::Values read;
        if (const json::Value *distance = entry.get("max_optimized_distance"))
            read.mMaxOptimizedDistance = (float) distance->number(read.mMaxOptimizedDistance);
        if (const json::Value *dropped = entry.get("max_dropped_ticks"))
            read.mMaxDroppedTicks = (int64_t) dropped->number((double) read.mMaxDroppedTicks);
        if (const json::Value *hints = entry.get("use_motion_prediction_hints"))
            read.mUseMotionPredictionHints = hints->boolean(false);
        values = read;
    }

    const json::Value *componentOf(const ServerActor &actor) {
        if (const MobActor *mob = dynamic_cast<const MobActor *>(&actor))
            return mob->getComponent(COMPONENT);
        const json::Value *definition = EntityDefinitions::find(actor.getIdentifier());
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        return components == nullptr ? nullptr : components->get(COMPONENT);
    }
}

bool BandwidthOptimization::valuesOf(ServerNetworkHandler &owner, const ServerActor &actor, Values &values) {
    const json::Value *component = componentOf(actor);
    if (component == nullptr)
        return false;

    values = Values();
    if (const json::Value *defaults = component->get("default_values"))
        readValues(*defaults, values);

    // the first entry whose filters hold for the actor replaces the defaults
    const json::Value *conditional = component->get("conditional_values");
    if (conditional == nullptr || !conditional->isArray())
        return true;
    for (const std::unique_ptr<json::Value> &entry: conditional->mArray) {
        const json::Value *filters = entry->get("conditional_values");
        if (filters != nullptr && !EntityFilter::test(*filters, owner, actor))
            continue;
        readValues(*entry, values);
        break;
    }
    return true;
}

int64_t BandwidthOptimization::allowedGap(const Values &values, float distance) {
    const float maxDistance = std::max(0.0f, values.mMaxOptimizedDistance);
    // the game divides 0 by 0 here: a viewer standing on a zero-distance actor gets no gap
    if (maxDistance <= 0.0f && distance <= 0.0f)
        return 0;
    const float ratio = maxDistance >= distance ? distance / maxDistance : 1.0f;
    return (int64_t) ((float) values.mMaxDroppedTicks * GAP_SCALE * ratio);
}
