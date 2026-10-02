#include "Actor/Definition/EntityFilter.h"

#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/FollowCaravanGoal.h"
#include "Actor/AI/Navigation/PathNavigation.h"
#include "Actor/ActorDamageSource.h"
#include "Actor/DamageCause.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Block/Systems/PrecipitationSystem.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Core/Math/Vector3i.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Generator/Overworld/Biome/ClimateAttributes.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

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

    const char *const NUMERIC_SUBJECTS[] = {"self", "other", "parent", "player", "target", "baby", "damager",
                                            "block"};
    const char *const NUMERIC_OPERATORS[] = {"==", "!=", "<", "<=", ">", ">="};

    std::string enumName(const json::Value *value, const char *const names[], size_t count,
                         const std::string &fallback) {
        if (value == nullptr)
            return fallback;
        if (!value->isNumber())
            return value->string();

        const int32_t index = value->integer(-1);
        return index >= 0 && (size_t) index < count ? names[index] : fallback;
    }

    int32_t difficultyValue(const json::Value &value) {
        return value.isNumber() ? value.integer(-1) : difficultyOf(value.string());
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

    bool isBoundToHomeBlock(ServerNetworkHandler &owner, const MobActor &mob) {
        if (!mob.hasHome())
            return false;

        const Vector3f &home = mob.getHomePosition();
        const int32_t x = (int32_t) std::floor(home.x);
        const int32_t y = (int32_t) std::floor(home.y);
        const int32_t z = (int32_t) std::floor(home.z);
        const BlockState *state = owner.getLevelFor(mob).peekBlockPtr(x, y, z);
        if (state == nullptr)
            return true;

        const Block *block = VanillaBlocks::fromIdentifier(state->mName);
        return block != nullptr && block->bindsHomeActors();
    }

    std::vector<const ItemStack *> equipmentOf(const Actor &actor, const std::string &domain) {
        std::vector<const ItemStack *> items;
        if (const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor)) {
            const PlayerInventory &inventory = player->getInventory();
            const bool any = domain == "any";
            if (any || domain == "hand")
                items.push_back(&inventory.getItemInHand());
            if (any || domain == "offhand")
                items.push_back(&inventory.getOffhand());
            if (domain == "head")
                items.push_back(&inventory.getArmor(PlayerInventory::ARMOR_HEAD));
            if (domain == "torso")
                items.push_back(&inventory.getArmor(PlayerInventory::ARMOR_TORSO));
            if (domain == "leg")
                items.push_back(&inventory.getArmor(PlayerInventory::ARMOR_LEGS));
            if (domain == "feet")
                items.push_back(&inventory.getArmor(PlayerInventory::ARMOR_FEET));
            if (any || domain == "armor") {
                for (const ItemStack &armor: inventory.getArmorContents())
                    items.push_back(&armor);
            }
            return items;
        }

        const MobActor *mob = dynamic_cast<const MobActor *>(&actor);
        if (mob == nullptr)
            return items;

        MobEquipment &equipment = const_cast<MobActor *>(mob)->getEquipment();
        const bool any = domain == "any";
        if (any || domain == "hand")
            items.push_back(&equipment.getSlot(MobEquipment::MAINHAND));
        if (any || domain == "offhand")
            items.push_back(&equipment.getSlot(MobEquipment::OFFHAND));
        if (domain == "head")
            items.push_back(&equipment.getSlot(MobEquipment::HEAD));
        if (domain == "torso")
            items.push_back(&equipment.getSlot(MobEquipment::CHEST));
        if (domain == "leg")
            items.push_back(&equipment.getSlot(MobEquipment::LEGS));
        if (domain == "feet")
            items.push_back(&equipment.getSlot(MobEquipment::FEET));
        if (any || domain == "body")
            items.push_back(&equipment.getSlot(MobEquipment::BODY));
        if (any || domain == "armor") {
            for (int slot = MobEquipment::HEAD; slot <= MobEquipment::BODY; ++slot)
                items.push_back(&equipment.getSlot(slot));
        }
        if (any || domain == "inventory") {
            items.push_back(&equipment.getSlot(MobEquipment::BODY));
            for (int slot = 0; slot < equipment.getInventorySize(); ++slot)
                items.push_back(&equipment.getInventoryItem(slot));
        }
        return items;
    }

    bool hasEquipmentIn(const Actor &actor, const std::string &domain, const BehaviorItems &items) {
        for (const ItemStack *item: equipmentOf(actor, domain)) {
            if (items.contains(*item))
                return true;
        }
        return false;
    }

    bool hasEquipmentTagIn(const Actor &actor, const std::string &domain, const std::string &tag) {
        for (const ItemStack *item: equipmentOf(actor, domain)) {
            if (item->isAir() || item->mDefinition == nullptr)
                continue;

            const std::vector<std::string> &tags = CraftingRecipeTable::getItemTags(
                    std::string(item->mDefinition->getIdentifier()));
            if (std::find(tags.begin(), tags.end(), tag) != tags.end())
                return true;
        }
        return false;
    }

    std::vector<std::string> vehicleFamiliesOf(ServerNetworkHandler &owner, const Actor &actor) {
        const Actor *vehicle = RideSystem::resolve(owner, actor.getVehicleId());
        return vehicle == nullptr ? std::vector<std::string>() : familiesOf(*vehicle);
    }

    std::vector<std::string> controllerFamiliesOf(ServerNetworkHandler &owner, const Actor &actor) {
        const std::vector<int64_t> &passengers = actor.getPassengers();
        const Actor *controller = passengers.empty() ? nullptr : RideSystem::resolve(owner, passengers.front());
        return controller == nullptr ? std::vector<std::string>() : familiesOf(*controller);
    }

    bool containsFamily(const std::vector<std::string> &families, const json::Value *value) {
        return value != nullptr && std::find(families.begin(), families.end(), value->string()) != families.end();
    }

    const char *const COLOR_NAMES[] = {"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink",
                                       "gray", "silver", "cyan", "purple", "blue", "brown", "green", "red",
                                       "black"};
    const char *const RANGED_WEAPONS[] = {"minecraft:bow", "minecraft:crossbow"};
    const char *const HOT_BLOCK = "minecraft:magma";
    const char *const FATAL_DAMAGE = "fatal";
    const float NO_PLAYER_DISTANCE = 1.0e9f;
    const int32_t TICKS_PER_SECOND = 20;

    const ActorDamageSource *activeDamage = nullptr;
    float activeDamageAmount = 0.0f;

    std::string equipmentDomain(const json::Value *domain) {
        if (domain == nullptr || !domain->isString())
            return "any";

        const std::string name = domain->string();
        return name == "main_hand" ? "hand" : name;
    }

    std::string withNamespace(const std::string &name) {
        return name.find(':') == std::string::npos ? "minecraft:" + name : name;
    }

    std::string itemIdentifier(const ItemStack &item) {
        return item.isAir() || item.mDefinition == nullptr ? std::string()
                                                           : std::string(item.mDefinition->getIdentifier());
    }

    bool isRainingOn(Level &level, const Vector3i &position) {
        if (!level.isRaining() || position.y < level.getHeightAt(position.x, position.z))
            return false;

        LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
        const ClimateAttributes *climate = chunk == nullptr ? nullptr
                                           : ClimateAttributes::getForBiome(
                        (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15));
        return climate != nullptr && climate->mRain && !PrecipitationSystem::isCold(level, position);
    }

    float temperatureAt(Level &level, const Vector3i &position) {
        LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
        const ClimateAttributes *climate = chunk == nullptr ? nullptr
                                           : ClimateAttributes::getForBiome(
                        (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15));
        return climate == nullptr ? 0.5f : climate->mTemperature;
    }

    int32_t redstoneStrengthAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        int32_t strength = 0;
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            strength = std::max(strength, RedstoneSystem::getRedstonePower(
                    owner, level, RedstoneFace::relative(position, face), face));
        }
        return strength;
    }

    float nearestPlayerDistance(ServerNetworkHandler &owner, const Actor &actor) {
        float nearest = NO_PLAYER_DISTANCE;
        const Vector3f position = actor.getPosition();
        for (auto &entry: owner.getPlayers()) {
            const ServerPlayer &player = entry.second;
            if (!player.isSpawned() || &player == &actor || player.getDimension() != actor.getDimension())
                continue;

            const Vector3f other = player.getPosition();
            const float dx = other.x - position.x;
            const float dy = other.y - position.y;
            const float dz = other.z - position.z;
            nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        return nearest;
    }

    bool hasAbility(const ServerPlayer &player, const std::string &ability) {
        const int32_t gameType = player.getGameType();
        const bool creative = gameType == (int32_t) GameType::Creative;
        const bool spectator = gameType == (int32_t) GameType::Spectator;
        if (ability == "instabuild")
            return creative;
        if (ability == "invulnerable" || ability == "mayfly")
            return creative || spectator;
        if (ability == "flying")
            return player.isFlying();
        if (ability == "operator_commands" || ability == "teleport")
            return player.isOp();
        return false;
    }

    int32_t componentValue(const Actor &actor, const char *component) {
        const MobActor *mob = dynamic_cast<const MobActor *>(&actor);
        const json::Value *data = mob == nullptr ? nullptr : mob->getComponent(component);
        const json::Value *value = data == nullptr ? nullptr : data->get("value");
        return value == nullptr ? -1 : value->integer(-1);
    }
}

EntityFilter::DamageScope::DamageScope(const ActorDamageSource &source, float amount) {
    activeDamage = &source;
    activeDamageAmount = amount;
}

EntityFilter::DamageScope::~DamageScope() {
    activeDamage = nullptr;
    activeDamageAmount = 0.0f;
}

bool EntityFilter::test(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                        const Actor *other) {
    if (filter.isArray())
        return _testAll(filter, owner, self, other);
    if (const json::Value *all = filter.get("all_of"))
        return _testAll(*all, owner, self, other);
    if (const json::Value *all = filter.get("AND"))
        return _testAll(*all, owner, self, other);
    if (const json::Value *any = filter.get("any_of"))
        return _testAny(*any, owner, self, other);
    if (const json::Value *any = filter.get("OR"))
        return _testAny(*any, owner, self, other);
    if (const json::Value *none = filter.get("none_of"))
        return !_testAny(*none, owner, self, other);
    if (const json::Value *none = filter.get("NOT"))
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

bool EntityFilter::_testBlock(const std::string &test, const std::string &op, const json::Value *value,
                              ServerNetworkHandler &owner, const MobActor &self) {
    if (!self.hasEventBlock())
        return false;

    Level &level = owner.getLevelFor(self);
    const Vector3i &position = self.getEventBlock();
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr)
        return false;

    if (test == "is_block") {
        const std::string expected = value == nullptr ? std::string() : value->string();
        const bool result = !expected.empty()
                            && (state->mName == expected || state->mName == "minecraft:" + expected);
        return isNegation(op) ? !result : result;
    }

    if (test == "is_waterlogged") {
        const BlockState *liquid = level.peekBlockPtr(position.x, position.y, position.z, 1);
        return applyBoolean(liquid != nullptr && LiquidView(*liquid).isWater(), value, op);
    }

    return false;
}

bool EntityFilter::_testSingle(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                               const Actor *other) {
    const json::Value *testValue = filter.get("test");
    if (testValue == nullptr)
        return true;

    const std::string test = testValue->string();
    const std::string op = enumName(filter.get("operator"), NUMERIC_OPERATORS,
                                    sizeof(NUMERIC_OPERATORS) / sizeof(NUMERIC_OPERATORS[0]), "==");
    const json::Value *value = filter.get("value");
    const std::string subject = enumName(filter.get("subject"), NUMERIC_SUBJECTS,
                                         sizeof(NUMERIC_SUBJECTS) / sizeof(NUMERIC_SUBJECTS[0]), "self");
    const Actor *target = &self;
    if (subject == "other" || subject == "damager" || subject == "player")
        target = other;
    else if (subject == "target")
        target = self.getTarget(owner);

    if (subject == "block")
        return _testBlock(test, op, value, owner, self);

    if (test == "is_difficulty") {
        const int32_t expected = value == nullptr ? -1 : difficultyValue(*value);
        return expected >= 0 && compareNumbers((int32_t) owner.getProperties().getDifficulty(), expected, op);
    }

    if (test == "random_chance") {
        const int32_t bound = value == nullptr ? 1 : std::max(1, value->integer(1));
        const bool result = std::uniform_int_distribution<int32_t>(0, bound - 1)(filterRandom()) == 0;
        return applyBoolean(result, nullptr, op);
    }

    if (test == "hourly_clock_time") {
        const int64_t timeOfDay = ((owner.getLevelFor(self).getTime() % DAY_LENGTH) + DAY_LENGTH) % DAY_LENGTH;
        return value != nullptr && compareNumbers(timeOfDay, (int64_t) value->number(0.0), op);
    }

    if (test == "has_target")
        return applyBoolean(self.getTarget(owner) != nullptr, value, op);

    if (test == "target_distance") {
        const Actor *mobTarget = self.getTarget(owner);
        return mobTarget != nullptr && value != nullptr
               && compareFloats(std::sqrt(self.distanceSquaredTo(*mobTarget)), (float) value->number(0.0), op);
    }

    if (test == "y_rotation") {
        const float yaw = std::remainder(self.getRotation().y, 360.0f);
        return value != nullptr && compareFloats(yaw, (float) value->number(0.0), op);
    }

    if (test == "is_game_rule") {
        const json::Value *domain = filter.get("domain");
        if (domain == nullptr)
            return false;

        const GameRules &rules = owner.getLevel().getGameRules();
        if (value != nullptr && value->isNumber())
            return compareNumbers(rules.getInt(domain->string()), (int64_t) value->number(0.0), op);
        return applyBoolean(rules.getBool(domain->string()), value, op);
    }

    if (test == "has_damage") {
        if (activeDamage == nullptr || value == nullptr)
            return false;

        const std::string cause = value->string();
        const bool result = cause == FATAL_DAMAGE ? activeDamageAmount >= self.getHealth()
                                                  : DamageCause::matches(cause, activeDamage->mDeathMessageKey);
        return isNegation(op) ? !result : result;
    }

    if (target == nullptr)
        return false;

    if (test == "actor_health")
        return value != nullptr && compareNumbers((int64_t) std::ceil(target->getHealth()),
                                                  (int64_t) value->number(0.0), op);

    if (test == "rider_count")
        return value != nullptr && compareNumbers((int64_t) target->getPassengers().size(),
                                                  (int64_t) value->number(0.0), op);

    if (test == "has_nametag") {
        const ServerActor *named = dynamic_cast<const ServerActor *>(target);
        return applyBoolean(named != nullptr && !named->getNameTag().empty(), value, op);
    }

    if (test == "is_family") {
        const bool result = containsFamily(familiesOf(*target), value);
        return isNegation(op) ? !result : result;
    }

    if (test == "is_vehicle_family") {
        const bool result = containsFamily(vehicleFamiliesOf(owner, *target), value);
        return isNegation(op) ? !result : result;
    }

    if (test == "is_controlling_passenger_family") {
        const bool result = containsFamily(controllerFamiliesOf(owner, *target), value);
        return isNegation(op) ? !result : result;
    }

    if (test == "is_sneak_held" || test == "is_sneaking")
        return applyBoolean(target->getFlags().get(ActorFlag::Sneaking), value, op);

    if (test == "is_sitting") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        return applyBoolean(mob != nullptr && (mob->isSitting() || mob->getFlags().get(ActorFlag::Sitting)), value,
                            op);
    }

    if (test == "is_baby") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        return applyBoolean(mob != nullptr && mob->getComponent("minecraft:is_baby") != nullptr, value, op);
    }

    if (test == "is_variant" || test == "is_mark_variant") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        const json::Value *component = mob == nullptr ? nullptr
                                       : mob->getComponent(test == "is_variant" ? "minecraft:variant"
                                                                                : "minecraft:mark_variant");
        const json::Value *current = component == nullptr ? nullptr : component->get("value");
        return value != nullptr && compareNumbers((int64_t) (current == nullptr ? 0 : current->integer(0)),
                                                  (int64_t) value->number(0.0), op);
    }

    if (test == "bool_property" || test == "enum_property" || test == "int_property" || test == "float_property") {
        const ServerActor *actor = dynamic_cast<const ServerActor *>(target);
        const json::Value *domain = filter.get("domain");
        const ActorPropertyDescription *descriptor = actor == nullptr || domain == nullptr
                                                     ? nullptr : actor->findPropertyDescription(domain->string());
        if (descriptor == nullptr)
            return false;

        const std::string &name = descriptor->mName;
        if (test == "bool_property")
            return applyBoolean(actor->getIntProperty(name, descriptor->mDefaultInt) != 0, value, op);

        if (test == "enum_property") {
            const int32_t index = actor->getIntProperty(name, descriptor->mDefaultInt);
            const bool result = descriptor->mType == ActorPropertyDescription::Type::Enum && value != nullptr
                                && index == descriptor->findEnumIndex(value->string());
            return isNegation(op) ? !result : result;
        }

        if (test == "float_property")
            return value != nullptr && compareFloats(actor->getFloatProperty(name, descriptor->mDefaultFloat),
                                                     (float) value->number(0.0), op);

        return value != nullptr && compareNumbers(actor->getIntProperty(name, descriptor->mDefaultInt),
                                                  (int64_t) value->number(0.0), op);
    }

    if (test == "home_distance") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        if (mob == nullptr || !mob->hasHome() || value == nullptr)
            return false;

        const Vector3f position = mob->getPosition();
        const Vector3f &home = mob->getHomePosition();
        const float dx = position.x - home.x;
        const float dy = position.y - home.y;
        const float dz = position.z - home.z;
        return compareFloats(std::sqrt(dx * dx + dy * dy + dz * dz), (float) value->number(0.0), op);
    }

    if (test == "is_bound_to_creaking_heart") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        return applyBoolean(mob != nullptr && isBoundToHomeBlock(owner, *mob), value, op);
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

    if (test == "is_panicking") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        return applyBoolean(mob != nullptr && mob->isPanicking(), value, op);
    }

    if (test == "owner_distance") {
        const MobActor *mob = dynamic_cast<const MobActor *>(target);
        const ServerPlayer *mobOwner = mob == nullptr ? nullptr : mob->getOwner(owner);
        return mobOwner != nullptr && value != nullptr
               && compareFloats(std::sqrt(mob->distanceSquaredTo(*mobOwner)), (float) value->number(0.0), op);
    }

    if (test == "in_nether")
        return applyBoolean(target->getDimension() == DimensionType::Nether, value, op);

    if (test == "has_container_open") {
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(target);
        return applyBoolean(player != nullptr && player->getInventoryManager().isContainerOpen(), value, op);
    }

    if (test == "has_silk_touch") {
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(target);
        const bool result = player != nullptr && ItemEnchantments::getLevel(player->getInventory().getItemInHand(),
                                                                            EnchantmentIds::SILK_TOUCH) > 0;
        return applyBoolean(result, value, op);
    }

    Level &level = owner.getLevelFor(*target);
    const Vector3i position = blockPosition(*target);

    if (test == "weather") {
        const std::string weather = value == nullptr ? std::string() : value->string();
        bool result = false;
        if (weather == "clear")
            result = !level.isRaining();
        else if (weather == "precipitation")
            result = level.isRaining();
        else if (weather == "thunderstorm")
            result = level.isThundering();
        else
            return false;
        return isNegation(op) ? !result : result;
    }

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
        const json::Value *domain = filter.get("domain");
        const bool result = value != nullptr
                            && hasEquipmentIn(*target, domain == nullptr ? "any" : domain->string(),
                                              BehaviorItems(value));
        return isNegation(op) ? !result : result;
    }

    if (test == "has_equipment_tag") {
        const json::Value *domain = filter.get("domain");
        const bool result = value != nullptr
                            && hasEquipmentTagIn(*target, domain == nullptr ? "any" : domain->string(),
                                                 value->string());
        return isNegation(op) ? !result : result;
    }

    if (test == "is_snow_covered") {
        const bool result = level.getBlockState(position.x, position.y, position.z).mName == "minecraft:snow_layer";
        return applyBoolean(result, value, op);
    }

    const MobActor *targetMob = dynamic_cast<const MobActor *>(target);
    const ServerPlayer *targetPlayer = dynamic_cast<const ServerPlayer *>(target);

    if (test == "on_ground")
        return applyBoolean(target->isOnGround(), value, op);

    if (test == "on_fire")
        return applyBoolean(target->isOnFire(), value, op);

    if (test == "is_riding")
        return applyBoolean(target->isRiding(), value, op);

    if (test == "is_riding_self")
        return applyBoolean(target->getVehicleId() == self.getUniqueId(), value, op);

    if (test == "is_sprinting")
        return applyBoolean(target->getFlags().get(ActorFlag::Sprinting), value, op);

    if (test == "is_moving") {
        const Vector3f motion = target->getMotion();
        const bool moving = target->getFlags().get(ActorFlag::Moving) || motion.x * motion.x + motion.z * motion.z
                                                                           > 1.0e-6f;
        return applyBoolean(moving, value, op);
    }

    if (test == "is_sleeping") {
        const bool sleeping = targetPlayer != nullptr ? targetPlayer->isSleeping()
                                                      : target->getFlags().get(ActorFlag::Sleeping);
        return applyBoolean(sleeping, value, op);
    }

    if (test == "is_leashed")
        return applyBoolean(target->getFlags().get(ActorFlag::Leashed), value, op);

    if (test == "is_visible")
        return applyBoolean(!target->hasEffect(MobEffectId::Invisibility), value, op);

    if (test == "is_missing_health")
        return applyBoolean(target->getHealth() < target->getMaxHealth(), value, op);

    if (test == "is_persistent") {
        const ServerActor *actor = dynamic_cast<const ServerActor *>(target);
        return applyBoolean(actor != nullptr && actor->isPersistent(), value, op);
    }

    if (test == "is_tamed")
        return applyBoolean(targetMob != nullptr && targetMob->isTamed(), value, op);

    if (test == "is_owner")
        return applyBoolean(self.isOwnedBy(*target), value, op);

    if (test == "was_last_hurt_by")
        return applyBoolean(self.getLastHurtBy() != 0 && self.getLastHurtBy() == target->getRuntimeId(), value, op);

    if (test == "is_avoiding_mobs")
        return applyBoolean(targetMob != nullptr && targetMob->isAvoidingMobs(), value, op);

    if (test == "is_navigating")
        return applyBoolean(targetMob != nullptr && !targetMob->getNavigation().isDone(), value, op);

    if (test == "inactivity_timer") {
        const int32_t seconds = targetMob == nullptr ? 0 : targetMob->getInactivityTicks() / TICKS_PER_SECOND;
        return value != nullptr && compareNumbers(seconds, (int64_t) value->number(0.0), op == "==" ? ">=" : op);
    }

    if (test == "has_ability") {
        const bool result = targetPlayer != nullptr && value != nullptr && hasAbility(*targetPlayer, value->string());
        return isNegation(op) ? !result : result;
    }

    if (test == "has_mob_effect") {
        MobEffectId effect;
        const bool result = value != nullptr && parseDefinitionMobEffect(value->string(), effect)
                            && target->hasEffect(effect);
        return isNegation(op) ? !result : result;
    }

    if (test == "is_color") {
        const int32_t color = componentValue(*target, "minecraft:color");
        const bool result = value != nullptr && color >= 0 && color < 16 && value->string() == COLOR_NAMES[color];
        return isNegation(op) ? !result : result;
    }

    if (test == "is_skin_id")
        return value != nullptr && compareNumbers(componentValue(*target, "minecraft:skin_id"),
                                                  (int64_t) value->number(0.0), op);

    if (test == "has_ranged_weapon") {
        const std::vector<const ItemStack *> hand = equipmentOf(*target, "hand");
        const std::string held = hand.empty() ? std::string() : itemIdentifier(*hand.front());
        const bool result = std::find(std::begin(RANGED_WEAPONS), std::end(RANGED_WEAPONS), held)
                            != std::end(RANGED_WEAPONS);
        return applyBoolean(result, value, op);
    }

    if (test == "all_slots_empty") {
        bool empty = true;
        for (const ItemStack *item: equipmentOf(*target, equipmentDomain(value)))
            empty = empty && item->isAir();
        return isNegation(op) ? !empty : empty;
    }

    if (test == "has_damaged_equipment") {
        const std::string expected = value == nullptr ? std::string() : withNamespace(value->string());
        bool result = false;
        for (const ItemStack *item: equipmentOf(*target, equipmentDomain(filter.get("domain"))))
            result = result || (item->mDamage > 0 && (expected.empty() || itemIdentifier(*item) == expected));
        return isNegation(op) ? !result : result;
    }

    if (test == "actor_has_item_with_enchantment_in_slot") {
        const EnchantmentData *enchantment = value == nullptr ? nullptr
                                                              : EnchantmentData::findByName(value->string());
        bool result = false;
        if (enchantment != nullptr) {
            for (const ItemStack *item: equipmentOf(*target, equipmentDomain(filter.get("domain"))))
                result = result || ItemEnchantments::getLevel(*item, enchantment->mId) > 0;
        }
        return isNegation(op) ? !result : result;
    }

    if (test == "has_same_equipment_in_slot_as") {
        const std::string domain = equipmentDomain(filter.get("domain"));
        const std::vector<const ItemStack *> mine = equipmentOf(self, domain);
        const std::vector<const ItemStack *> theirs = equipmentOf(*target, domain);
        const bool result = !mine.empty() && !theirs.empty() && !mine.front()->isAir()
                            && itemIdentifier(*mine.front()) == itemIdentifier(*theirs.front());
        return applyBoolean(result, value, op);
    }

    if (test == "distance_to_nearest_player")
        return value != nullptr && compareFloats(nearestPlayerDistance(owner, *target), (float) value->number(0.0),
                                                 op);

    if (test == "in_lava" || test == "in_contact_with_water" || test == "in_water_or_rain") {
        const LiquidContact contact = LiquidBlocksFetch::at(level, target->getPosition());
        bool result = test == "in_lava" ? contact.lava : contact.water;
        if (test != "in_lava")
            result = result || isRainingOn(level, position);
        return applyBoolean(result, value, op);
    }

    if (test == "in_block") {
        const std::string expected = value == nullptr ? std::string() : withNamespace(value->string());
        const bool result = level.getBlockState(position.x, position.y, position.z).mName == expected;
        return isNegation(op) ? !result : result;
    }

    if (test == "on_hot_block") {
        const bool result = level.getBlockState(position.x, position.y - 1, position.z).mName == HOT_BLOCK;
        return applyBoolean(result, value, op);
    }

    if (test == "taking_fire_damage") {
        const bool burning = target->isOnFire() || LiquidBlocksFetch::at(level, target->getPosition()).lava;
        return applyBoolean(burning && !target->isFireImmune() && !target->hasEffect(MobEffectId::FireResistance),
                            value, op);
    }

    if (test == "is_biome") {
        LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
        const int32_t biome = chunk == nullptr ? -1
                                               : (int32_t) chunk->getBiomeAt(position.x & 15, position.y,
                                                                              position.z & 15);
        const bool result = value != nullptr && BiomeChunkGenDataRegistry::hasTag(biome, value->string());
        return isNegation(op) ? !result : result;
    }

    if (test == "is_temperature_value")
        return value != nullptr && compareFloats(temperatureAt(level, position), (float) value->number(0.0), op);

    if (test == "weather_at_position") {
        const std::string weather = value == nullptr ? std::string() : value->string();
        const bool exposed = position.y >= level.getHeightAt(position.x, position.z);
        const bool cold = PrecipitationSystem::isCold(level, position);
        bool result;
        if (weather == "clear")
            result = !level.isRaining() || !exposed;
        else if (weather == "rain")
            result = isRainingOn(level, position);
        else if (weather == "snow")
            result = level.isRaining() && exposed && cold;
        else if (weather == "precipitation")
            result = level.isRaining() && exposed;
        else if (weather == "thunderstorm")
            result = level.isThundering() && exposed;
        else
            return false;
        return isNegation(op) ? !result : result;
    }

    if (test == "redstone_strength_at_position")
        return value != nullptr && compareNumbers(redstoneStrengthAt(owner, level, position),
                                                  (int64_t) value->number(0.0), op);

    if (test == "in_caravan")
        return applyBoolean(FollowCaravanGoal::isInCaravan(target->getUniqueId()), value, op);

    if (test == "is_raider" || test == "is_in_village" || test == "has_trade_supply" || test == "trusts"
        || test == "is_leashed_to")
        return applyBoolean(false, value, op);

    return false;
}
