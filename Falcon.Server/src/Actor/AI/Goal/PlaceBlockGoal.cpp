#include "Actor/AI/Goal/PlaceBlockGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Block/Block.h"
#include "Block/BlockPaletteRegistry.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/BlockStateUpgrades.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>
#include <string>
#include <utility>

namespace {
    const char *const AIR = "minecraft:air";
    const char *const GRIEFING_RULE = "mobgriefing";
    const float DEFAULT_CHANCE = 1.0f;

    std::mt19937 &placeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomIn(const PlaceBlockGoal::Range &range) {
        if (range.mMax <= range.mMin)
            return range.mMin;
        return std::uniform_int_distribution<int32_t>(range.mMin, range.mMax)(placeRandom());
    }

    PlaceBlockGoal::Range rangeOf(const json::Value *value) {
        PlaceBlockGoal::Range range;
        if (value == nullptr)
            return range;

        if (value->isObject()) {
            const json::Value *minimum = value->get("min");
            const json::Value *maximum = value->get("max");
            range.mMin = minimum == nullptr ? 0 : minimum->integer(0);
            range.mMax = maximum == nullptr ? range.mMin : maximum->integer(range.mMin);
            return range;
        }

        if (value->isArray()) {
            if (value->mArray.empty())
                return range;
            range.mMin = value->mArray.front()->integer(0);
            range.mMax = value->mArray.back()->integer(range.mMin);
            return range;
        }

        range.mMin = value->integer(0);
        range.mMax = range.mMin;
        return range;
    }

    std::shared_ptr<json::Value> cloneOf(const json::Value *value) {
        return value == nullptr ? nullptr : std::shared_ptr<json::Value>(value->clone());
    }

    void applyState(Tag &states, const std::string &name, const json::Value &value) {
        const Tag *current = states.get(name);
        if (current == nullptr)
            return;

        if (current->getType() == Tag::Type::String)
            states.putString(name, value.string(current->asString()));
        else if (current->getType() == Tag::Type::Byte)
            states.putByte(name, (int8_t) (value.isNumber() ? value.integer(0) : (value.boolean(false) ? 1 : 0)));
        else if (current->getType() == Tag::Type::Int)
            states.putInt(name, value.integer(current->asInt()));
    }

    bool blockStateOf(const json::Value *descriptor, BlockState &out) {
        if (descriptor == nullptr)
            return false;

        const json::Value *nameValue = descriptor->isString() ? descriptor : descriptor->get("name");
        if (nameValue == nullptr || nameValue->string().empty())
            return false;

        const std::string name = BlockStateUpgrades::currentName(nameValue->string());
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        if (defaults == nullptr)
            return false;

        Tag states = *defaults;
        const json::Value *overrides = descriptor->isObject() ? descriptor->get("states") : nullptr;
        if (overrides != nullptr && overrides->isObject()) {
            for (const std::string &key: overrides->mKeys)
                applyState(states, key, *overrides->mObject.at(key));
        }

        out = BlockState(name, states);
        return true;
    }
}

PlaceBlockGoal::PlaceBlockGoal(std::vector<Entry> entries, std::shared_ptr<json::Value> canPlace, float chance,
                               Range xzRange, Range yRange, bool affectedByGriefingRule,
                               std::shared_ptr<json::Value> onPlace)
        : mEntries(std::move(entries)), mCanPlace(std::move(canPlace)), mChance(chance), mXzRange(xzRange),
          mYRange(yRange), mAffectedByGriefingRule(affectedByGriefingRule), mOnPlace(std::move(onPlace)) {
}

std::unique_ptr<Goal> PlaceBlockGoal::create(const json::Value &component) {
    std::vector<Entry> entries;
    const json::Value *blocks = component.get("randomly_placeable_blocks");
    if (blocks != nullptr && blocks->isArray()) {
        for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
            Entry entry;
            if (!blockStateOf(block->get("block"), entry.mState))
                continue;
            entry.mFilter = cloneOf(block->get("filter"));
            entries.push_back(std::move(entry));
        }
    }

    if (entries.empty())
        return nullptr;

    const json::Value *chance = component.get("chance");
    const json::Value *griefing = component.get("affected_by_griefing_rule");
    return std::make_unique<PlaceBlockGoal>(std::move(entries), cloneOf(component.get("can_place")),
                                            chance == nullptr ? DEFAULT_CHANCE
                                                              : (float) chance->number(DEFAULT_CHANCE),
                                            rangeOf(component.get("xz_range")), rangeOf(component.get("y_range")),
                                            griefing == nullptr || griefing->boolean(true),
                                            cloneOf(component.get("on_place")));
}

bool PlaceBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(placeRandom()) >= mChance)
        return false;

    Level &level = owner.getLevelFor(mob);
    if (mAffectedByGriefingRule && !level.getGameRules().getBool(GRIEFING_RULE))
        return false;

    if (mCanPlace != nullptr && !EntityFilter::test(*mCanPlace, owner, mob))
        return false;

    return _findPlacement(owner, level, mob);
}

bool PlaceBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void PlaceBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    level.setBlock(mPosition, mState, true);

    if (mOnPlace == nullptr)
        return;

    mob.setEventBlock(mPosition);
    EntityEvents::fireTrigger(owner, mob, mOnPlace.get());
    mob.clearEventBlock();
}

bool PlaceBlockGoal::_findPlacement(ServerNetworkHandler &owner, Level &level, MobActor &mob) {
    const Vector3f feet = mob.getPosition();
    const Vector3i position((int32_t) std::floor(feet.x) + randomIn(mXzRange),
                            (int32_t) std::floor(feet.y) + randomIn(mYRange),
                            (int32_t) std::floor(feet.z) + randomIn(mXzRange));

    const BlockState *current = level.peekBlockPtr(position.x, position.y, position.z);
    if (current == nullptr || current->mName != AIR)
        return false;

    std::vector<const Entry *> candidates;
    for (const Entry &entry: mEntries) {
        if (entry.mFilter == nullptr || EntityFilter::test(*entry.mFilter, owner, mob))
            candidates.push_back(&entry);
    }

    if (candidates.empty())
        return false;

    const size_t index = std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(placeRandom());
    const BlockState &state = candidates[index]->mState;
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (block == nullptr || !block->canSurvive(level, position, state))
        return false;

    mPosition = position;
    mState = state;
    return true;
}
