#include "Actor/Definition/EntityFilter.h"

#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/Blocks/LiquidView.h"
#include "Core/Math/Vector3i.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
    const int64_t DAY_LENGTH = 24000;
    const int64_t DAYTIME_END = 12000;
    const float MAX_LIGHT = 15.0f;
    const char *const EFFECT_COMPONENT_PREFIX = "minecraft:effect.";

    std::mt19937 &filterRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t difficultyOf(const std::string &name) {
        if (name == "peaceful")
            return 0;
        if (name == "easy")
            return 1;
        if (name == "normal")
            return 2;
        if (name == "hard")
            return 3;
        return -1;
    }

    bool isNegation(const std::string &op) {
        return op == "!=" || op == "not" || op == "<>";
    }

    bool compareNumbers(int64_t left, int64_t right, const std::string &op) {
        if (op == "<")
            return left < right;
        if (op == ">")
            return left > right;
        if (op == "<=")
            return left <= right;
        if (op == ">=")
            return left >= right;
        if (isNegation(op))
            return left != right;
        return left == right;
    }

    bool compareFloats(float left, float right, const std::string &op) {
        if (op == "<")
            return left < right;
        if (op == ">")
            return left > right;
        if (op == "<=")
            return left <= right;
        if (op == ">=")
            return left >= right;
        if (isNegation(op))
            return left != right;
        return left == right;
    }

    bool applyBoolean(bool result, const json::Value *value, const std::string &op) {
        const bool expected = value == nullptr || value->boolean(true);
        return isNegation(op) ? result != expected : result == expected;
    }

    std::vector<std::string> familiesOf(const Actor &actor) {
        if (actor.isPlayer())
            return {"player"};

        if (const MobActor *mob = dynamic_cast<const MobActor *>(&actor))
            return mob->getFamilies();

        std::vector<std::string> families;
        const ServerActor *serverActor = dynamic_cast<const ServerActor *>(&actor);
        const json::Value *definition = serverActor == nullptr ? nullptr
                                                               : EntityDefinitions::find(serverActor->getIdentifier());
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *typeFamily = components == nullptr ? nullptr : components->get("minecraft:type_family");
        const json::Value *family = typeFamily == nullptr ? nullptr : typeFamily->get("family");
        if (family != nullptr) {
            for (const std::unique_ptr<json::Value> &entry: family->mArray)
                families.push_back(entry->string());
        }
        return families;
    }

    Vector3i blockPosition(const Actor &actor) {
        const Vector3f position = actor.getPosition();
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

bool EntityFilter::test(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                        const Actor *other) {
    if (filter.isArray())
        return _testAll(filter, owner, self, other);
    if (const json::Value *all = filter.get("all_of"))
        return _testAll(*all, owner, self, other);
    if (const json::Value *any = filter.get("any_of"))
        return _testAny(*any, owner, self, other);
    if (const json::Value *none = filter.get("none_of"))
        return !_testAny(*none, owner, self, other);
    return _testSingle(filter, owner, self, other);
}

bool EntityFilter::_testAll(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                            const Actor *other) {
    if (!filters.isArray())
        return test(filters, owner, self, other);

    for (const std::unique_ptr<json::Value> &entry: filters.mArray) {
        if (!test(*entry, owner, self, other))
            return false;
    }
    return true;
}

bool EntityFilter::_testAny(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                            const Actor *other) {
    if (!filters.isArray())
        return test(filters, owner, self, other);

    for (const std::unique_ptr<json::Value> &entry: filters.mArray) {
        if (test(*entry, owner, self, other))
            return true;
    }
    return false;
}

bool EntityFilter::_testSingle(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                               const Actor *other) {
    const json::Value *testValue = filter.get("test");
    if (testValue == nullptr)
        return true;

    const std::string test = testValue->string();
    const json::Value *operation = filter.get("operator");
    const std::string op = operation == nullptr ? "==" : operation->string();
    const json::Value *value = filter.get("value");
    const json::Value *subjectValue = filter.get("subject");
    const std::string subject = subjectValue == nullptr ? "self" : subjectValue->string();
    const Actor *target = subject == "other" || subject == "target" || subject == "damager" ? other : &self;

    if (test == "is_difficulty") {
        const int32_t expected = value == nullptr ? -1 : difficultyOf(value->string());
        return expected >= 0 && compareNumbers((int32_t) owner.getProperties().getDifficulty(), expected, op);
    }

    if (test == "random_chance") {
        const int32_t bound = value == nullptr ? 1 : std::max(1, value->integer(1));
        const bool result = std::uniform_int_distribution<int32_t>(0, bound - 1)(filterRandom()) == 0;
        return applyBoolean(result, nullptr, op);
    }

    if (target == nullptr)
        return false;

    if (test == "is_family") {
        const std::vector<std::string> families = familiesOf(*target);
        const bool result = value != nullptr
                            && std::find(families.begin(), families.end(), value->string()) != families.end();
        return isNegation(op) ? !result : result;
    }

    if (test == "has_component" && value != nullptr && value->string().rfind(EFFECT_COMPONENT_PREFIX, 0) == 0) {
        MobEffectId effect;
        const std::string name = value->string().substr(std::string(EFFECT_COMPONENT_PREFIX).size());
        const bool result = parseDefinitionMobEffect(name, effect) && target->hasEffect(effect);
        return isNegation(op) ? !result : result;
    }

    if (test == "has_component") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        const bool result = mob != nullptr && value != nullptr && mob->getComponent(value->string()) != nullptr;
        return isNegation(op) ? !result : result;
    }

    Level &level = owner.getLevelFor(*target);
    const Vector3i position = blockPosition(*target);

    if (test == "has_biome_tag") {
        LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
        const int32_t biome = chunk == nullptr ? -1
                                               : (int32_t) chunk->getBiomeAt(position.x & 15, position.y,
                                                                              position.z & 15);
        const bool result = value != nullptr && BiomeChunkGenDataRegistry::hasTag(biome, value->string());
        return isNegation(op) ? !result : result;
    }

    if (test == "is_daytime")
        return applyBoolean(((level.getTime() % DAY_LENGTH) + DAY_LENGTH) % DAY_LENGTH < DAYTIME_END, value, op);

    if (test == "in_water" || test == "is_underwater") {
        const int32_t y = test == "is_underwater" ? (int32_t) std::floor(target->getPosition().y + 1.0f) : position.y;
        const bool result = LiquidView(level.getBlockState(position.x, y, position.z)).isWater();
        return applyBoolean(result, value, op);
    }

    if (test == "is_brightness") {
        const int32_t blockLight = level.getBlockLightAt(position.x, position.y, position.z);
        const int32_t skyLight = level.getSkyLightAt(position.x, position.y, position.z)
                                 - level.getSkyLightSubtracted();
        const float brightness = (float) std::max(blockLight, skyLight) / MAX_LIGHT;
        return value != nullptr && compareFloats(brightness, (float) value->number(0.0), op);
    }

    if (test == "is_underground")
        return applyBoolean(position.y < level.getHeightAt(position.x, position.z), value, op);

    if (test == "has_equipment") {
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(target);
        const json::Value *domain = filter.get("domain");
        const bool hand = domain == nullptr || domain->string() == "hand" || domain->string() == "any";
        const bool result = player != nullptr && hand && value != nullptr
                            && BehaviorItems(value).contains(player->getInventory().getItemInHand());
        return isNegation(op) ? !result : result;
    }

    if (test == "is_snow_covered") {
        const bool result = level.getBlockState(position.x, position.y, position.z).mName == "minecraft:snow_layer";
        return applyBoolean(result, value, op);
    }

    return false;
}
