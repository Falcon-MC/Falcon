#include "JsonEntityFilter.h"

#include "EntityDefinitionsJson.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "Scripting/BehaviorPackManager.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Passive/CowActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const char *const GROUP_KEYS[] = {"test", "all_of", "AND", "any_of", "OR", "none_of", "NOT"};

    const char *const IDENTIFIERS[] = {
            "minecraft:zombie",   "minecraft:cow",          "minecraft:villager_v2", "minecraft:wolf",
            "minecraft:cat",      "minecraft:creeper",      "minecraft:skeleton",    "minecraft:fox",
            "minecraft:bee",      "minecraft:piglin",       "minecraft:horse",       "minecraft:sheep",
            "minecraft:warden",   "minecraft:axolotl",      "minecraft:goat",        "minecraft:iron_golem",
            "minecraft:panda",    "minecraft:llama",        "minecraft:hoglin",      "minecraft:allay",
            "minecraft:frog",     "minecraft:enderman",     "minecraft:drowned",     "minecraft:pillager",
            "minecraft:zombie_pigman", "minecraft:armor_stand", "minecraft:boat",    "minecraft:painting"};

    bool looksLikeFilter(const json::Value &value) {
        if (value.isArray()) {
            for (const std::unique_ptr<json::Value> &entry: value.mArray) {
                if (looksLikeFilter(*entry))
                    return true;
            }
            return false;
        }

        for (const char *key: GROUP_KEYS) {
            if (value.get(key) != nullptr)
                return true;
        }
        return false;
    }

    // random_chance draws from a generator of its own in each implementation: left out
    bool drawsRandom(const json::Value &value) {
        if (value.isString() && value.mString == "random_chance")
            return true;
        for (const std::unique_ptr<json::Value> &entry: value.mArray) {
            if (drawsRandom(*entry))
                return true;
        }
        for (const auto &entry: value.mObject) {
            if (drawsRandom(*entry.second))
                return true;
        }
        return false;
    }

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void compiledComponentsMatch(const json::Value &root) {
        using Definitions = EntityDefinitions;
        using Id = Definitions::ComponentId;
        const auto checkGroup = [](const json::Value *data, const Definitions::ComponentGroup &compiled) {
            const auto matches = [](const std::optional<float> &number, const json::Value *field) {
                return field != nullptr && field->isNumber() ? number == (float) field->number()
                                                              : !number.has_value();
            };
            require(compiled.size() == (data == nullptr ? 0 : data->mKeys.size()), "component group size changed");
            for (size_t i = 0; i < compiled.size(); ++i) {
                const auto &entry = compiled[i];
                const std::string &name = data->mKeys[i];
                const auto *raw = data->get(name);
                require(entry.mId == Definitions::findComponentId(name) && entry.mJson == raw,
                        "component lookup changed for " + name);
                if (entry.mId == (uint32_t) Id::TypeFamily) {
                    std::vector<std::string> families;
                    if (const auto *list = raw->get("family")) {
                        for (const auto &family: list->mArray)
                            families.push_back(family->string());
                    }
                    require(std::get<std::vector<std::string>>(entry.mFields) == families, "compiled families differ");
                } else if (entry.mId == (uint32_t) Id::Inventory) {
                    const auto &fields = std::get<Definitions::Inventory>(entry.mFields);
                    const auto *size = raw->get("inventory_size");
                    const auto *slots = raw->get("additional_slots_per_strength");
                    const auto *type = raw->get("container_type");
                    require(fields.mSize == (size == nullptr ? 5.0f : (float) size->number(5))
                            && fields.mSlotsPerStrength == (slots == nullptr ? 0.0f : (float) slots->number())
                            && fields.mMount == (type != nullptr && type->string() == "horse"),
                            "compiled inventory differs");
                } else if (entry.mId == (uint32_t) Id::Equippable) {
                    const auto &fields = std::get<Definitions::Equippable>(entry.mFields);
                    const auto *slots = raw->get("slots");
                    const size_t count = slots == nullptr || !slots->isArray() ? 0 : slots->mArray.size();
                    require(fields.mSlots == count, "compiled equippable slots differ");
                } else if (entry.mId == (uint32_t) Id::Strength) {
                    const auto &fields = std::get<Definitions::Strength>(entry.mFields);
                    const auto *value = raw->get("value");
                    const auto *maximum = raw->get("max");
                    require(fields.mValue == (value == nullptr ? 0.0f : (float) value->number())
                            && fields.mMaximum == (maximum == nullptr ? 5.0f : (float) maximum->number(5)),
                            "compiled strength differs");
                } else if (entry.mId == (uint32_t) Id::CollisionBox) {
                    const auto &box = std::get<Definitions::CollisionBox>(entry.mFields);
                    require(matches(box.mWidth, raw->get("width")) && matches(box.mHeight, raw->get("height")),
                            "compiled collision box differs");
                } else if (entry.mId == (uint32_t) Id::Movement || entry.mId == (uint32_t) Id::JumpStrength
                           || entry.mId == (uint32_t) Id::FollowRange) {
                    const auto &fields = std::get<Definitions::NumericValue>(entry.mFields);
                    const auto *value = raw->get("value");
                    const bool range = value != nullptr && value->isObject();
                    require(fields.mRange == range, "compiled numeric form differs");
                    if (range)
                        require(matches(fields.mMinimum, value->get("range_min"))
                                && matches(fields.mMaximum, value->get("range_max")), "compiled range differs");
                    else
                        require(matches(fields.mValue, value), "compiled number differs");
                }
            }
        };
        for (const auto &name: root.mKeys) {
            const auto *raw = Definitions::find(name);
            const auto *compiled = Definitions::findCompiled(raw);
            require(raw != nullptr && compiled != nullptr, "definition did not compile: " + name);
            checkGroup(raw->get("components"), compiled->mBase);
            const auto *groups = raw->get("component_groups");
            require(compiled->mGroups.size() == (groups == nullptr ? 0 : groups->mKeys.size()),
                    "compiled group count differs for " + name);
            if (groups != nullptr) {
                for (const auto &group: groups->mKeys)
                    checkGroup(groups->get(group), compiled->mGroups.at(group));
            }
        }
    }

    // every filter of a definition: goals, sensors, events and the rest, nested ones included
    void collect(const json::Value &value, std::vector<const json::Value *> &filters) {
        if (!value.isArray() && !value.isObject())
            return;
        if (looksLikeFilter(value) && !drawsRandom(value))
            filters.push_back(&value);
        for (const std::unique_ptr<json::Value> &entry: value.mArray)
            collect(*entry, filters);
        for (const auto &entry: value.mObject)
            collect(*entry.second, filters);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Entity filter parity", "", 1};
        Level *mLevel = nullptr;
        std::vector<ServerActor *> mActors;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "parity.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 4; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }

            float x = 0.5f;
            for (const char *identifier: IDENTIFIERS) {
                mLevel->setBlockState((int32_t) x, 69, 0, BlockState("minecraft:stone"));
                if (ServerActor *actor = mOwner.spawnActor(*mLevel, identifier, Vector3f(x, 70.0f, 0.5f)))
                    mActors.push_back(actor);
                x += 2.0f;
            }
            if (mActors.size() < 20)
                throw std::runtime_error("the arena actors did not spawn");
        }

        MobActor *mob(size_t index) const {
            return dynamic_cast<MobActor *>(mActors[index % mActors.size()]);
        }
    };

    void packComponentPrecedence(Arena &arena, const std::filesystem::path &directory) {
        const auto packRoot = directory / "typed-pack";
        std::filesystem::create_directories(packRoot / "entities");
        std::ofstream(packRoot / "entities" / "test.json") << R"({
            "minecraft:entity": {
                "description": {"identifier": "test:typed"},
                "components": {
                    "minecraft:type_family": {"family": ["base"]},
                    "minecraft:inventory": {"inventory_size": 9},
                    "minecraft:movement": {"value": 0.25}
                },
                "component_groups": {
                    "first": {
                        "minecraft:type_family": {"family": ["first"]},
                        "minecraft:movement": {"value": 0.5}
                    },
                    "second": {
                        "minecraft:type_family": {"family": ["second"]},
                        "minecraft:inventory": {"inventory_size": 12}
                    }
                },
                "events": {"test:family": {"filters": {"test": "is_family", "value": "first"}}}
            }
        })";
        BehaviorPack pack;
        pack.mPath = packRoot.string();
        EntityDefinitions::loadPacks({pack});
        {
            CowActor actor(54321, "test:typed");
            const auto *definition = actor.getDefinition();
            require(definition != nullptr, "test pack entity did not load");
            const auto &filter = *definition->get("events")->get("test:family")->get("filters");
            require(actor.getMovementSpeed() == 0.25f && actor.getInventoryCapacity() == 9,
                    "base pack components were not selected");
            require(!EntityFilter::test(filter, arena.mOwner, actor), "the group filter matched the base family");
            actor.addComponentGroup("first");
            require(actor.getMovementSpeed() == 0.5f && EntityFilter::test(filter, arena.mOwner, actor),
                    "compiled fields were not applied with the group");
            actor.addComponentGroup("second");
            require(actor.getInventoryCapacity() == 12 && !EntityFilter::test(filter, arena.mOwner, actor),
                    "the last active group did not replace the family and inventory");
            actor.removeComponentGroup("second");
            require(actor.getInventoryCapacity() == 9 && EntityFilter::test(filter, arena.mOwner, actor),
                    "removing the last group did not restore the previous components");
            actor.removeComponentGroup("first");
            require(actor.getMovementSpeed() == 0.25f && actor.getTypeFamilies() == std::vector<std::string>{"base"},
                    "removing the groups did not restore the base");
        }
        EntityDefinitions::loadPacks({});
    }

    struct Totals {
        size_t mChecks = 0;
        size_t mPassed = 0;
        size_t mMismatches = 0;
    };

    // json and compiled filter on every self and other of the arena, the compiled one by definition and on its own
    void compare(Arena &arena, const std::vector<const json::Value *> &filters, const char *situation,
                 Totals &totals) {
        std::vector<const Actor *> others = {nullptr};
        for (size_t index = 0; index < arena.mActors.size(); index += 3)
            others.push_back(arena.mActors[index]);

        for (const json::Value *filter: filters) {
            const std::shared_ptr<const EntityFilter::Group> compiled = EntityFilter::compile(filter);
            for (ServerActor *self: arena.mActors) {
                for (const Actor *other: others) {
                    const bool expected = JsonEntityFilter::test(*filter, arena.mOwner, *self, other);
                    const bool byDefinition = EntityFilter::test(*filter, arena.mOwner, *self, other);
                    const bool alone = EntityFilter::test(*compiled, arena.mOwner, *self, other);
                    ++totals.mChecks;
                    totals.mPassed += expected ? 1 : 0;
                    if (byDefinition == expected && alone == expected)
                        continue;

                    if (++totals.mMismatches <= 10)
                        std::cout << situation << ": " << self->getIdentifier() << " against "
                                  << (other == nullptr ? "nobody" : other->getIdentifier()) << " expected "
                                  << expected << '\n';
                }
            }
        }
    }

    void situations(Arena &arena, const std::vector<const json::Value *> &filters, Totals &totals) {
        Level &level = *arena.mLevel;
        level.setTime(1000);
        compare(arena, filters, "day", totals);

        level.setTime(18000);
        level.setRainingState(true);
        level.setThunderingState(true);
        compare(arena, filters, "stormy night", totals);
        level.setRainingState(false);
        level.setThunderingState(false);

        for (size_t index = 0; index < arena.mActors.size(); index += 2) {
            ServerActor &actor = *arena.mActors[index];
            actor.setOnFire();
            actor.setNameTag("Named");
            actor.getFlags().set(ActorFlag::Sneaking, true);
            actor.getFlags().set(ActorFlag::Sprinting, true);
            MobEffectInstance invisibility;
            invisibility.mId = MobEffectId::Invisibility;
            invisibility.mDuration = 200;
            actor.addEffect(invisibility);
        }
        compare(arena, filters, "burning, named and sneaking", totals);

        for (size_t index = 0; index < arena.mActors.size(); ++index) {
            if (MobActor *mob = arena.mob(index)) {
                mob->fireEvent(arena.mOwner, "minecraft:entity_born");
                if (MobActor *target = arena.mob(index + 1))
                    mob->setTarget(arena.mOwner, target->getRuntimeId());
            }
        }
        compare(arena, filters, "babies with targets", totals);

        for (size_t index = 1; index < arena.mActors.size(); index += 3) {
            const Vector3f position = arena.mActors[index]->getPosition();
            level.setBlockState((int32_t) std::floor(position.x), 70, 0, BlockState("minecraft:water"));
        }
        ServerActor &attacker = *arena.mActors.front();
        const ActorDamageSource attack = ActorDamageSource::attack("death.attack.mob", "victim", attacker, "attacker",
                                                                   attacker.getPosition());
        const EntityFilter::DamageScope compiledDamage(attack, 4.0f);
        const JsonEntityFilter::DamageScope jsonDamage(attack, 4.0f);
        compare(arena, filters, "in water and hurt by a mob", totals);
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-filter-parity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();

        // the definitions themselves, not a copy: the compiled filters are found by their json
        const std::unique_ptr<json::Value> listing = json::parse(FalconEntityData::kEntityDefinitionsJson);
        if (listing == nullptr)
            throw std::runtime_error("the embedded definitions did not parse");
        compiledComponentsMatch(*listing);
        std::vector<const json::Value *> filters;
        for (const std::string &identifier: listing->mKeys) {
            if (const json::Value *definition = EntityDefinitions::find(identifier))
                collect(*definition, filters);
        }
        if (filters.size() < 1000)
            throw std::runtime_error("too few filters found in the definitions");

        Arena arena(directory);
        Totals totals;
        situations(arena, filters, totals);
        packComponentPrecedence(arena, directory);
        std::cout << filters.size() << " filters, " << totals.mChecks << " checks, " << totals.mPassed
                  << " passed, " << totals.mMismatches << " mismatches\n";
        if (totals.mMismatches != 0)
            throw std::runtime_error("a compiled filter disagreed with the json filter");
        if (totals.mPassed == 0 || totals.mPassed == totals.mChecks)
            throw std::runtime_error("the filters all gave the same answer");
        std::cout << "entity filter parity checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
