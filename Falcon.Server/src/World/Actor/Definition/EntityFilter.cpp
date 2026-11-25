#include "World/Actor/Definition/EntityFilter.h"

#include "World/Actor/AI/Goal/BehaviorItems.h"
#include "World/Actor/AI/Goal/FollowCaravanGoal.h"
#include "World/Actor/AI/Navigation/PathNavigation.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/RideSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "Core/Math/Vector3i.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    const int64_t DAY_LENGTH = Level::TICKS_PER_DAY;
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

    const std::string NUMERIC_SUBJECTS[] = {"self", "other", "parent", "player", "target", "baby", "damager", "block"};
    const std::string NUMERIC_OPERATORS[] = {"==", "!=", "<", "<=", ">", ">="};

    const std::string &textOf(const json::Value &value) {
        static const std::string empty;
        return value.isString() ? value.mString : empty;
    }

    const std::string &enumName(const json::Value *value, const std::string names[], size_t count,
                                const std::string &fallback) {
        if (value == nullptr)
            return fallback;
        if (!value->isNumber())
            return textOf(*value);

        const int32_t index = value->integer(-1);
        return index >= 0 && (size_t) index < count ? names[index] : fallback;
    }

    int32_t difficultyValue(const json::Value &value) {
        return value.isNumber() ? value.integer(-1) : difficultyOf(value.string());
    }

    const char *const PLAYER_FAMILY = "player";
    const char *const TYPE_FAMILY_COMPONENT = "minecraft:type_family";

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

    const MobActor *mobOf(const Actor &actor) {
        return actor.hasTypeFlag(ActorTypeFlag::MobFlagComponent) ? static_cast<const MobActor *>(&actor) : nullptr;
    }

    const ServerPlayer *playerOf(const Actor &actor) {
        return actor.hasTypeFlag(ActorTypeFlag::PlayerComponent) ? static_cast<const ServerPlayer *>(&actor)
                                                                  : nullptr;
    }

    // players and item actors are the only actors that are not server actors
    const ServerActor *serverActorOf(const Actor &actor) {
        return actor.hasTypeFlag(ActorTypeFlag::PlayerComponent)
                               || actor.hasTypeFlag(ActorTypeFlag::ItemActorFlagComponent)
                       ? nullptr
                       : static_cast<const ServerActor *>(&actor);
    }

    // only a mob has a target
    const Actor *targetOf(ServerNetworkHandler &owner, const ServerActor &self) {
        const MobActor *mob = mobOf(self);
        return mob == nullptr ? nullptr : mob->getTarget(owner);
    }

    float distanceSquared(const Actor &from, const Actor &to) {
        const Vector3f &a = from.getPosition();
        const Vector3f &b = to.getPosition();
        return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    }

    std::vector<const ItemStack *> equipmentOf(const Actor &actor, const std::string &domain) {
        std::vector<const ItemStack *> items;
        if (const ServerPlayer *player = playerOf(actor)) {
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

        const MobActor *mob = mobOf(actor);
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

    const Actor *vehicleOf(ServerNetworkHandler &owner, const Actor &actor) {
        return RideSystem::resolve(owner, actor.getVehicleId());
    }

    const Actor *controllerOf(ServerNetworkHandler &owner, const Actor &actor) {
        const std::vector<int64_t> &passengers = actor.getPassengers();
        return passengers.empty() ? nullptr : RideSystem::resolve(owner, passengers.front());
    }

    const char *const COLOR_NAMES[] = {"white",  "orange", "magenta", "light_blue", "yellow", "lime",  "pink", "gray",
                                       "silver", "cyan",   "purple",  "blue",       "brown",  "green", "red",  "black"};
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

    float temperatureAt(Level &level, const Vector3i &position) {
        LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
        const ClimateAttributes *climate = chunk == nullptr
                                                   ? nullptr
                                                   : ClimateAttributes::getForBiome((int32_t) chunk->getBiomeAt(
                                                             position.x & 15, position.y, position.z & 15));
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
            if (!player.isSpawned() || &player == &actor || !player.sharesLevelWith(actor))
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
        if (ability == "invulnerable")
            return creative || spectator;
        if (ability == "mayfly")
            return player.canToggleFlight();
        if (ability == "flying")
            return player.isFlying();
        if (ability == "operator_commands" || ability == "teleport")
            return player.isOp();
        return false;
    }

    int32_t componentValue(const Actor &actor, const char *component) {
        const MobActor *mob = mobOf(actor);
        const json::Value *data = mob == nullptr ? nullptr : mob->getComponent(component);
        const json::Value *value = data == nullptr ? nullptr : data->get("value");
        return value == nullptr ? -1 : value->integer(-1);
    }
}

// one test of a filter: its keys and value read once, when it is compiled
struct EntityFilter::Test {
    enum class Type : uint8_t {
        Unknown,
        IsDifficulty,
        RandomChance,
        HourlyClockTime,
        HasTarget,
        TargetDistance,
        YRotation,
        IsGameRule,
        HasDamage,
        IsTarget,
        ActorHealth,
        RiderCount,
        HasNametag,
        IsFamily,
        IsVehicleFamily,
        IsControllingPassengerFamily,
        IsSneaking,
        IsSitting,
        IsBaby,
        IsVariant,
        IsMarkVariant,
        BoolProperty,
        EnumProperty,
        IntProperty,
        FloatProperty,
        HomeDistance,
        IsBoundToCreakingHeart,
        HasComponent,
        IsPanicking,
        OwnerDistance,
        InNether,
        InOverworld,
        HasContainerOpen,
        HasSilkTouch,
        Weather,
        HasBiomeTag,
        IsDaytime,
        InWater,
        IsUnderwater,
        IsBrightness,
        IsUnderground,
        HasEquipment,
        HasEquipmentTag,
        IsSnowCovered,
        OnGround,
        OnFire,
        IsRiding,
        IsRidingSelf,
        IsSprinting,
        IsMoving,
        IsSleeping,
        IsLeashed,
        IsVisible,
        IsMissingHealth,
        IsPersistent,
        IsTamed,
        IsOwner,
        WasLastHurtBy,
        IsAvoidingMobs,
        IsNavigating,
        InactivityTimer,
        HasAbility,
        HasMobEffect,
        IsColor,
        IsSkinId,
        HasRangedWeapon,
        AllSlotsEmpty,
        HasDamagedEquipment,
        HasItemWithEnchantmentInSlot,
        HasSameEquipmentInSlotAs,
        DistanceToNearestPlayer,
        InLava,
        InContactWithWater,
        InWaterOrRain,
        InBlock,
        OnHotBlock,
        TakingFireDamage,
        IsBiome,
        IsTemperatureValue,
        WeatherAtPosition,
        RedstoneStrengthAtPosition,
        InCaravan,
        IsInVillage,
        Unsupported,
        IsBlock,
        IsWaterlogged
    };

    enum class Operator : uint8_t { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

    enum class Subject : uint8_t { Self, Other, Target, Block };

    bool mHasTest = false;
    Type mType = Type::Unknown;
    Operator mOperator = Operator::Equal;
    // the operator spelled "==" or left out: inactivity_timer reads it as >=
    bool mExactEquals = false;
    Subject mSubject = Subject::Self;
    bool mHasValue = false;
    bool mValueIsNumber = false;
    double mNumber = 0.0;
    // value missing or true; what the boolean tests compare against
    bool mExpected = true;
    std::string mText;
    // the namespaced text; the block form of is_block
    std::string mQualified;
    int32_t mInteger = 0;
    bool mHasDomain = false;
    std::string mDomain;
    std::string mEquipmentDomain;
    // has_equipment resolves its items on each test: the item names may not be loaded yet at compile time
    std::shared_ptr<const json::Value> mItems;
    // has_component on minecraft:effect.<name>
    bool mEffectComponent = false;
    bool mHasEffect = false;
    MobEffectId mEffect = MobEffectId::Speed;
};

struct EntityFilter::Group {
    enum class Mode : uint8_t { All, Any, None, Single };

    Mode mMode = Mode::Single;
    std::vector<Group> mChildren;
    Test mTest;
};

namespace {
    using FilterTest = EntityFilter::Test;
    using FilterTestType = EntityFilter::Test::Type;
    using FilterOperator = EntityFilter::Test::Operator;
    using FilterSubject = EntityFilter::Test::Subject;
    using FilterNode = EntityFilter::Group;
    using FilterMode = EntityFilter::Group::Mode;

    struct FilterTestName {
        const char *mName;
        FilterTestType mTest;
    };

    const FilterTestName FILTER_TESTS[] = {
            {"is_difficulty", FilterTestType::IsDifficulty},
            {"random_chance", FilterTestType::RandomChance},
            {"hourly_clock_time", FilterTestType::HourlyClockTime},
            {"has_target", FilterTestType::HasTarget},
            {"target_distance", FilterTestType::TargetDistance},
            {"y_rotation", FilterTestType::YRotation},
            {"is_game_rule", FilterTestType::IsGameRule},
            {"has_damage", FilterTestType::HasDamage},
            {"is_target", FilterTestType::IsTarget},
            {"actor_health", FilterTestType::ActorHealth},
            {"rider_count", FilterTestType::RiderCount},
            {"has_nametag", FilterTestType::HasNametag},
            {"is_family", FilterTestType::IsFamily},
            {"is_vehicle_family", FilterTestType::IsVehicleFamily},
            {"is_controlling_passenger_family", FilterTestType::IsControllingPassengerFamily},
            {"is_sneak_held", FilterTestType::IsSneaking},
            {"is_sneaking", FilterTestType::IsSneaking},
            {"is_sitting", FilterTestType::IsSitting},
            {"is_baby", FilterTestType::IsBaby},
            {"is_variant", FilterTestType::IsVariant},
            {"is_mark_variant", FilterTestType::IsMarkVariant},
            {"bool_property", FilterTestType::BoolProperty},
            {"enum_property", FilterTestType::EnumProperty},
            {"int_property", FilterTestType::IntProperty},
            {"float_property", FilterTestType::FloatProperty},
            {"home_distance", FilterTestType::HomeDistance},
            {"is_bound_to_creaking_heart", FilterTestType::IsBoundToCreakingHeart},
            {"has_component", FilterTestType::HasComponent},
            {"is_panicking", FilterTestType::IsPanicking},
            {"owner_distance", FilterTestType::OwnerDistance},
            {"in_nether", FilterTestType::InNether},
            {"in_overworld", FilterTestType::InOverworld},
            {"has_container_open", FilterTestType::HasContainerOpen},
            {"has_silk_touch", FilterTestType::HasSilkTouch},
            {"weather", FilterTestType::Weather},
            {"has_biome_tag", FilterTestType::HasBiomeTag},
            {"is_daytime", FilterTestType::IsDaytime},
            {"in_water", FilterTestType::InWater},
            {"is_underwater", FilterTestType::IsUnderwater},
            {"is_brightness", FilterTestType::IsBrightness},
            {"is_underground", FilterTestType::IsUnderground},
            {"has_equipment", FilterTestType::HasEquipment},
            {"has_equipment_tag", FilterTestType::HasEquipmentTag},
            {"is_snow_covered", FilterTestType::IsSnowCovered},
            {"on_ground", FilterTestType::OnGround},
            {"on_fire", FilterTestType::OnFire},
            {"is_riding", FilterTestType::IsRiding},
            {"is_riding_self", FilterTestType::IsRidingSelf},
            {"is_sprinting", FilterTestType::IsSprinting},
            {"is_moving", FilterTestType::IsMoving},
            {"is_sleeping", FilterTestType::IsSleeping},
            {"is_leashed", FilterTestType::IsLeashed},
            {"is_visible", FilterTestType::IsVisible},
            {"is_missing_health", FilterTestType::IsMissingHealth},
            {"is_persistent", FilterTestType::IsPersistent},
            {"is_tamed", FilterTestType::IsTamed},
            {"is_owner", FilterTestType::IsOwner},
            {"was_last_hurt_by", FilterTestType::WasLastHurtBy},
            {"is_avoiding_mobs", FilterTestType::IsAvoidingMobs},
            {"is_navigating", FilterTestType::IsNavigating},
            {"inactivity_timer", FilterTestType::InactivityTimer},
            {"has_ability", FilterTestType::HasAbility},
            {"has_mob_effect", FilterTestType::HasMobEffect},
            {"is_color", FilterTestType::IsColor},
            {"is_skin_id", FilterTestType::IsSkinId},
            {"has_ranged_weapon", FilterTestType::HasRangedWeapon},
            {"all_slots_empty", FilterTestType::AllSlotsEmpty},
            {"has_damaged_equipment", FilterTestType::HasDamagedEquipment},
            {"actor_has_item_with_enchantment_in_slot", FilterTestType::HasItemWithEnchantmentInSlot},
            {"has_same_equipment_in_slot_as", FilterTestType::HasSameEquipmentInSlotAs},
            {"distance_to_nearest_player", FilterTestType::DistanceToNearestPlayer},
            {"in_lava", FilterTestType::InLava},
            {"in_contact_with_water", FilterTestType::InContactWithWater},
            {"in_water_or_rain", FilterTestType::InWaterOrRain},
            {"in_block", FilterTestType::InBlock},
            {"on_hot_block", FilterTestType::OnHotBlock},
            {"taking_fire_damage", FilterTestType::TakingFireDamage},
            {"is_biome", FilterTestType::IsBiome},
            {"is_temperature_value", FilterTestType::IsTemperatureValue},
            {"weather_at_position", FilterTestType::WeatherAtPosition},
            {"redstone_strength_at_position", FilterTestType::RedstoneStrengthAtPosition},
            {"in_caravan", FilterTestType::InCaravan},
            {"is_in_village", FilterTestType::IsInVillage},
            {"is_raider", FilterTestType::Unsupported},
            {"has_trade_supply", FilterTestType::Unsupported},
            {"trusts", FilterTestType::Unsupported},
            {"is_leashed_to", FilterTestType::Unsupported},
            {"is_block", FilterTestType::IsBlock},
            {"is_waterlogged", FilterTestType::IsWaterlogged}};


    FilterOperator operatorOf(const json::Value *value, bool &exactEquals) {
        const std::string &op = enumName(value, NUMERIC_OPERATORS,
                                         sizeof(NUMERIC_OPERATORS) / sizeof(NUMERIC_OPERATORS[0]),
                                         NUMERIC_OPERATORS[0]);
        exactEquals = op == "==";
        if (op == "<")
            return FilterOperator::Less;
        if (op == ">")
            return FilterOperator::Greater;
        if (op == "<=")
            return FilterOperator::LessEqual;
        if (op == ">=")
            return FilterOperator::GreaterEqual;
        if (op == "!=" || op == "not" || op == "<>")
            return FilterOperator::NotEqual;
        return FilterOperator::Equal;
    }

    FilterSubject subjectOf(const json::Value *value) {
        const std::string &subject = enumName(value, NUMERIC_SUBJECTS,
                                              sizeof(NUMERIC_SUBJECTS) / sizeof(NUMERIC_SUBJECTS[0]),
                                              NUMERIC_SUBJECTS[0]);
        if (subject == "other" || subject == "damager" || subject == "player")
            return FilterSubject::Other;
        if (subject == "target")
            return FilterSubject::Target;
        if (subject == "block")
            return FilterSubject::Block;
        return FilterSubject::Self;
    }

    FilterTestType testOf(const std::string &name) {
        for (const FilterTestName &entry: FILTER_TESTS) {
            if (name == entry.mName)
                return entry.mTest;
        }
        return FilterTestType::Unknown;
    }

    FilterTest compileTest(const json::Value &filter) {
        FilterTest single;
        const json::Value *testValue = nullptr;
        const json::Value *operatorValue = nullptr;
        const json::Value *subjectValue = nullptr;
        const json::Value *domain = nullptr;
        const json::Value *value = nullptr;
        for (const auto &entry: filter.mObject) {
            const std::string &key = entry.first;
            if (key == "test")
                testValue = entry.second.get();
            else if (key == "operator")
                operatorValue = entry.second.get();
            else if (key == "value")
                value = entry.second.get();
            else if (key == "subject")
                subjectValue = entry.second.get();
            else if (key == "domain")
                domain = entry.second.get();
        }
        if (testValue == nullptr)
            return single;

        const std::string &name = textOf(*testValue);
        single.mHasTest = true;
        single.mType = testOf(name);
        single.mOperator = operatorOf(operatorValue, single.mExactEquals);
        single.mSubject = subjectOf(subjectValue);
        single.mHasValue = value != nullptr;
        single.mValueIsNumber = value != nullptr && value->isNumber();
        single.mNumber = value == nullptr ? 0.0 : value->number(0.0);
        single.mExpected = value == nullptr || value->boolean(true);
        single.mText = value == nullptr ? std::string() : value->string();
        single.mHasDomain = domain != nullptr;
        single.mDomain = domain == nullptr ? std::string() : domain->string();
        single.mEquipmentDomain = equipmentDomain(single.mType == FilterTestType::AllSlotsEmpty ? value : domain);

        switch (single.mType) {
            case FilterTestType::IsDifficulty:
                single.mInteger = value == nullptr ? -1 : difficultyValue(*value);
                break;
            case FilterTestType::RandomChance:
                single.mInteger = value == nullptr ? 1 : std::max(1, value->integer(1));
                break;
            case FilterTestType::IsBlock:
                single.mQualified = "minecraft:" + single.mText;
                break;
            case FilterTestType::HasDamagedEquipment:
            case FilterTestType::InBlock:
                single.mQualified = value == nullptr ? std::string() : withNamespace(single.mText);
                break;
            case FilterTestType::HasComponent: {
                const std::string prefix = EFFECT_COMPONENT_PREFIX;
                if (value == nullptr || single.mText.rfind(prefix, 0) != 0)
                    break;
                // has_component on an effect reads the effect, not a definition component
                single.mEffectComponent = true;
                single.mHasEffect = parseDefinitionMobEffect(single.mText.substr(prefix.size()), single.mEffect);
                break;
            }
            case FilterTestType::HasEquipment:
                if (value != nullptr)
                    single.mItems = value->clone();
                break;
            case FilterTestType::HasMobEffect:
                single.mHasEffect = value != nullptr && parseDefinitionMobEffect(single.mText, single.mEffect);
                break;
            default:
                break;
        }
        return single;
    }

    bool isFilterGroup(const std::string &key) {
        return key == "all_of" || key == "AND" || key == "any_of" || key == "OR" || key == "none_of"
               || key == "NOT";
    }

    FilterNode compileNode(const json::Value &filter);

    // a group holding one filter instead of a list tests that filter alone
    FilterNode compileGroup(FilterMode mode, const json::Value &group) {
        FilterNode node;
        node.mMode = mode;
        if (!group.isArray()) {
            node.mChildren.push_back(compileNode(group));
            return node;
        }

        for (const std::unique_ptr<json::Value> &entry: group.mArray)
            node.mChildren.push_back(compileNode(*entry));
        return node;
    }

    FilterNode compileNode(const json::Value &filter) {
        if (filter.isArray())
            return compileGroup(FilterMode::All, filter);

        const size_t GROUP_COUNT = 6;
        const char *const GROUP_KEYS[GROUP_COUNT] = {"all_of", "AND", "any_of", "OR", "none_of", "NOT"};
        const FilterMode GROUP_MODES[GROUP_COUNT] = {FilterMode::All, FilterMode::All, FilterMode::Any,
                                                     FilterMode::Any, FilterMode::None, FilterMode::None};
        // every group present must hold: villager_v2 become_zombie pairs all_of with any_of
        FilterNode node;
        node.mMode = FilterMode::All;
        for (size_t index = 0; index < GROUP_COUNT; ++index) {
            const json::Value *group = filter.get(GROUP_KEYS[index]);
            if (group != nullptr)
                node.mChildren.push_back(compileGroup(GROUP_MODES[index], *group));
        }
        if (!node.mChildren.empty())
            return node;

        node.mMode = FilterMode::Single;
        node.mTest = compileTest(filter);
        return node;
    }

    bool looksLikeFilter(const json::Value &value) {
        if (value.isArray()) {
            for (const std::unique_ptr<json::Value> &entry: value.mArray) {
                if (looksLikeFilter(*entry))
                    return true;
            }
            return false;
        }

        for (const auto &entry: value.mObject) {
            if (entry.first == "test" || isFilterGroup(entry.first))
                return true;
        }
        return false;
    }

    std::unordered_map<const json::Value *, FilterNode> &compiledFilters() {
        static std::unordered_map<const json::Value *, FilterNode> filters;
        return filters;
    }

    void compileTree(const json::Value &value) {
        if (!value.isArray() && !value.isObject())
            return;
        if (looksLikeFilter(value))
            compiledFilters().emplace(&value, compileNode(value));
        for (const std::unique_ptr<json::Value> &entry: value.mArray)
            compileTree(*entry);
        for (const auto &entry: value.mObject)
            compileTree(*entry.second);
    }

    bool compareNumbers(int64_t left, int64_t right, FilterOperator op) {
        switch (op) {
            case FilterOperator::Less:
                return left < right;
            case FilterOperator::Greater:
                return left > right;
            case FilterOperator::LessEqual:
                return left <= right;
            case FilterOperator::GreaterEqual:
                return left >= right;
            case FilterOperator::NotEqual:
                return left != right;
            default:
                return left == right;
        }
    }

    bool compareFloats(float left, float right, FilterOperator op) {
        switch (op) {
            case FilterOperator::Less:
                return left < right;
            case FilterOperator::Greater:
                return left > right;
            case FilterOperator::LessEqual:
                return left <= right;
            case FilterOperator::GreaterEqual:
                return left >= right;
            case FilterOperator::NotEqual:
                return left != right;
            default:
                return left == right;
        }
    }

    bool applyBoolean(bool result, const FilterTest &filter) {
        return filter.mOperator == FilterOperator::NotEqual ? result != filter.mExpected
                                                            : result == filter.mExpected;
    }

    bool applyNegation(bool result, const FilterTest &filter) {
        return filter.mOperator == FilterOperator::NotEqual ? !result : result;
    }

    bool hasValue(const FilterTest &filter) {
        return filter.mHasValue;
    }

    float numberOf(const FilterTest &filter) {
        return (float) filter.mNumber;
    }

    int64_t wholeOf(const FilterTest &filter) {
        return (int64_t) filter.mNumber;
    }

    bool testBlock(const FilterTest &filter, ServerNetworkHandler &owner, const ServerActor &self) {
        const MobActor *mob = mobOf(self);
        if (mob == nullptr || !mob->hasEventBlock())
            return false;

        Level &level = owner.getLevelFor(self);
        const Vector3i &position = mob->getEventBlock();
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        if (state == nullptr)
            return false;

        if (filter.mType == FilterTestType::IsBlock) {
            const bool result = !filter.mText.empty()
                                && (state->mName == filter.mText || state->mName == filter.mQualified);
            return applyNegation(result, filter);
        }

        if (filter.mType == FilterTestType::IsWaterlogged) {
            const BlockState *liquid = level.peekBlockPtr(position.x, position.y, position.z, 1);
            return applyBoolean(liquid != nullptr && LiquidView(*liquid).isWater(), filter);
        }

        return false;
    }

    // the tests that read the level at the subject's block
    bool testAtBlock(const FilterTest &filter, ServerNetworkHandler &owner, const Actor &target) {
        Level &level = owner.getLevelFor(target);
        const Vector3i position = blockPosition(target);
        switch (filter.mType) {
            case FilterTestType::Weather: {
                bool result;
                if (filter.mText == "clear")
                    result = !level.isRaining();
                else if (filter.mText == "precipitation")
                    result = level.isRaining();
                else if (filter.mText == "thunderstorm")
                    result = level.isThundering();
                else
                    return false;
                return applyNegation(result, filter);
            }
            case FilterTestType::HasBiomeTag:
            case FilterTestType::IsBiome: {
                LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
                const int32_t biome = chunk == nullptr
                                              ? -1
                                              : (int32_t) chunk->getBiomeAt(position.x & 15, position.y,
                                                                            position.z & 15);
                const bool result = hasValue(filter) && BiomeChunkGenDataRegistry::hasTag(biome, filter.mText);
                return applyNegation(result, filter);
            }
            case FilterTestType::IsDaytime:
                return applyBoolean(((level.getTime() % DAY_LENGTH) + DAY_LENGTH) % DAY_LENGTH < DAYTIME_END,
                                    filter);
            case FilterTestType::InWater:
            case FilterTestType::IsUnderwater: {
                const int32_t y = filter.mType == FilterTestType::IsUnderwater
                                          ? (int32_t) std::floor(target.getPosition().y + 1.0f)
                                          : position.y;
                return applyBoolean(LiquidView(level.getBlockState(position.x, y, position.z)).isWater(), filter);
            }
            case FilterTestType::IsBrightness: {
                const int32_t blockLight = level.getBlockLightAt(position.x, position.y, position.z);
                const int32_t skyLight = level.getSkyLightAt(position.x, position.y, position.z)
                                         - level.getSkyLightSubtracted();
                const float brightness = (float) std::max(blockLight, skyLight) / MAX_LIGHT;
                return hasValue(filter) && compareFloats(brightness, numberOf(filter), filter.mOperator);
            }
            case FilterTestType::IsUnderground:
                return applyBoolean(position.y < level.getHeightAt(position.x, position.z), filter);
            case FilterTestType::IsSnowCovered:
                return applyBoolean(
                        level.getBlockState(position.x, position.y, position.z).mName == "minecraft:snow_layer",
                        filter);
            case FilterTestType::InLava:
            case FilterTestType::InContactWithWater:
            case FilterTestType::InWaterOrRain: {
                const LiquidContact contact = LiquidBlocksFetch::at(level, target.getPosition());
                bool result = filter.mType == FilterTestType::InLava ? contact.lava : contact.water;
                if (filter.mType != FilterTestType::InLava)
                    result = result || PrecipitationSystem::isRainingOn(level, position);
                return applyBoolean(result, filter);
            }
            case FilterTestType::InBlock:
                return applyNegation(
                        level.getBlockState(position.x, position.y, position.z).mName == filter.mQualified, filter);
            case FilterTestType::OnHotBlock:
                return applyBoolean(level.getBlockState(position.x, position.y - 1, position.z).mName == HOT_BLOCK,
                                    filter);
            case FilterTestType::TakingFireDamage: {
                const bool burning = target.isOnFire() || LiquidBlocksFetch::at(level, target.getPosition()).lava;
                return applyBoolean(burning && !target.isFireImmune()
                                            && !target.hasEffect(MobEffectId::FireResistance),
                                    filter);
            }
            case FilterTestType::IsTemperatureValue:
                return hasValue(filter)
                       && compareFloats(temperatureAt(level, position), numberOf(filter), filter.mOperator);
            case FilterTestType::WeatherAtPosition: {
                const bool exposed = position.y >= level.getHeightAt(position.x, position.z);
                const bool cold = PrecipitationSystem::isCold(level, position);
                bool result;
                if (filter.mText == "clear")
                    result = !level.isRaining() || !exposed;
                else if (filter.mText == "rain")
                    result = PrecipitationSystem::isRainingOn(level, position);
                else if (filter.mText == "snow")
                    result = level.isRaining() && exposed && cold;
                else if (filter.mText == "precipitation")
                    result = level.isRaining() && exposed;
                else if (filter.mText == "thunderstorm")
                    result = level.isThundering() && exposed;
                else
                    return false;
                return applyNegation(result, filter);
            }
            case FilterTestType::RedstoneStrengthAtPosition:
                return hasValue(filter)
                       && compareNumbers(redstoneStrengthAt(owner, level, position), wholeOf(filter),
                                         filter.mOperator);
            default:
                return false;
        }
    }

    bool matchesFamily(const Actor *actor, const FilterTest &filter) {
        return actor != nullptr && hasValue(filter) && EntityFilter::hasFamily(*actor, filter.mText);
    }

    bool testProperty(const FilterTest &filter, const Actor &target) {
        const ServerActor *actor = serverActorOf(target);
        const ActorPropertyDescription *descriptor = actor == nullptr || !filter.mHasDomain
                                                             ? nullptr
                                                             : actor->findPropertyDescription(filter.mDomain);
        if (descriptor == nullptr)
            return false;

        const std::string &name = descriptor->mName;
        switch (filter.mType) {
            case FilterTestType::BoolProperty:
                return applyBoolean(actor->getIntProperty(name, descriptor->mDefaultInt) != 0, filter);
            case FilterTestType::EnumProperty: {
                const int32_t index = actor->getIntProperty(name, descriptor->mDefaultInt);
                const bool result = descriptor->mType == ActorPropertyDescription::Type::Enum && hasValue(filter)
                                    && index == descriptor->findEnumIndex(filter.mText);
                return applyNegation(result, filter);
            }
            case FilterTestType::FloatProperty:
                return hasValue(filter)
                       && compareFloats(actor->getFloatProperty(name, descriptor->mDefaultFloat), numberOf(filter),
                                        filter.mOperator);
            default:
                return hasValue(filter)
                       && compareNumbers(actor->getIntProperty(name, descriptor->mDefaultInt), wholeOf(filter),
                                         filter.mOperator);
        }
    }

    bool testOne(const FilterTest &filter, ServerNetworkHandler &owner, const ServerActor &self,
                    const Actor *other) {
        if (!filter.mHasTest)
            return true;

        const Actor *target = &self;
        if (filter.mSubject == FilterSubject::Other)
            target = other;
        else if (filter.mSubject == FilterSubject::Target)
            target = targetOf(owner, self);

        if (filter.mSubject == FilterSubject::Block)
            return testBlock(filter, owner, self);

        // the tests on the world or on self alone, whatever the subject
        switch (filter.mType) {
            case FilterTestType::IsDifficulty:
                return filter.mInteger >= 0
                       && compareNumbers((int32_t) owner.getProperties().getDifficulty(), filter.mInteger,
                                         filter.mOperator);
            case FilterTestType::RandomChance: {
                const bool result = std::uniform_int_distribution<int32_t>(0, filter.mInteger - 1)(filterRandom())
                                    == 0;
                return applyNegation(result, filter);
            }
            case FilterTestType::HourlyClockTime: {
                const int64_t timeOfDay = ((owner.getLevelFor(self).getTime() % DAY_LENGTH) + DAY_LENGTH)
                                          % DAY_LENGTH;
                return hasValue(filter) && compareNumbers(timeOfDay, wholeOf(filter), filter.mOperator);
            }
            case FilterTestType::HasTarget:
                return applyBoolean(targetOf(owner, self) != nullptr, filter);
            case FilterTestType::TargetDistance: {
                const Actor *mobTarget = targetOf(owner, self);
                return mobTarget != nullptr && hasValue(filter)
                       && compareFloats(std::sqrt(distanceSquared(self, *mobTarget)), numberOf(filter),
                                        filter.mOperator);
            }
            case FilterTestType::YRotation: {
                const float yaw = std::remainder(self.getRotation().y, 360.0f);
                return hasValue(filter) && compareFloats(yaw, numberOf(filter), filter.mOperator);
            }
            case FilterTestType::IsGameRule: {
                if (!filter.mHasDomain)
                    return false;

                const GameRules &rules = owner.getWorldFor(self).getOverworld().getGameRules();
                if (filter.mValueIsNumber)
                    return compareNumbers(rules.getInt(filter.mDomain), wholeOf(filter), filter.mOperator);
                return applyBoolean(rules.getBool(filter.mDomain), filter);
            }
            case FilterTestType::HasDamage: {
                if (activeDamage == nullptr || !hasValue(filter))
                    return false;

                const bool result = filter.mText == FATAL_DAMAGE ? activeDamageAmount >= self.getHealth()
                                                                 : DamageCause::matches(filter.mText, *activeDamage);
                return applyNegation(result, filter);
            }
            default:
                break;
        }

        if (target == nullptr)
            return false;

        switch (filter.mType) {
            case FilterTestType::IsTarget:
                return applyBoolean(target == targetOf(owner, self), filter);
            case FilterTestType::ActorHealth:
                return hasValue(filter)
                       && compareNumbers((int64_t) std::ceil(target->getHealth()), wholeOf(filter), filter.mOperator);
            case FilterTestType::RiderCount:
                return hasValue(filter)
                       && compareNumbers((int64_t) target->getPassengers().size(), wholeOf(filter), filter.mOperator);
            case FilterTestType::HasNametag: {
                const ServerActor *named = dynamic_cast<const ServerActor *>(target);
                return applyBoolean(named != nullptr && !named->getNameTag().empty(), filter);
            }
            case FilterTestType::IsFamily:
                return applyNegation(matchesFamily(target, filter), filter);
            case FilterTestType::IsVehicleFamily:
                return applyNegation(matchesFamily(vehicleOf(owner, *target), filter), filter);
            case FilterTestType::IsControllingPassengerFamily:
                return applyNegation(matchesFamily(controllerOf(owner, *target), filter), filter);
            case FilterTestType::IsSneaking:
                return applyBoolean(target->getFlags().get(ActorFlag::Sneaking), filter);
            case FilterTestType::IsSitting: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && (mob->isSitting() || mob->getFlags().get(ActorFlag::Sitting)),
                                    filter);
            }
            case FilterTestType::IsBaby: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && mob->getComponent("minecraft:is_baby") != nullptr, filter);
            }
            case FilterTestType::IsVariant:
            case FilterTestType::IsMarkVariant: {
                const MobActor *mob = mobOf(*target);
                const char *name = filter.mType == FilterTestType::IsVariant ? "minecraft:variant"
                                                                             : "minecraft:mark_variant";
                const json::Value *component = mob == nullptr ? nullptr : mob->getComponent(name);
                const json::Value *current = component == nullptr ? nullptr : component->get("value");
                return hasValue(filter)
                       && compareNumbers((int64_t) (current == nullptr ? 0 : current->integer(0)), wholeOf(filter),
                                         filter.mOperator);
            }
            case FilterTestType::BoolProperty:
            case FilterTestType::EnumProperty:
            case FilterTestType::IntProperty:
            case FilterTestType::FloatProperty:
                return testProperty(filter, *target);
            case FilterTestType::HomeDistance: {
                const MobActor *mob = mobOf(*target);
                if (mob == nullptr || !mob->hasHome() || !hasValue(filter))
                    return false;

                const Vector3f position = mob->getPosition();
                const Vector3f &home = mob->getHomePosition();
                const float dx = position.x - home.x;
                const float dy = position.y - home.y;
                const float dz = position.z - home.z;
                return compareFloats(std::sqrt(dx * dx + dy * dy + dz * dz), numberOf(filter), filter.mOperator);
            }
            case FilterTestType::IsBoundToCreakingHeart: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && isBoundToHomeBlock(owner, *mob), filter);
            }
            case FilterTestType::HasComponent: {
                if (filter.mEffectComponent)
                    return applyNegation(filter.mHasEffect && target->hasEffect(filter.mEffect), filter);

                const MobActor *mob = mobOf(*target);
                return applyNegation(mob != nullptr && hasValue(filter) && mob->getComponent(filter.mText) != nullptr,
                                     filter);
            }
            case FilterTestType::IsPanicking: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && mob->isPanicking(), filter);
            }
            case FilterTestType::OwnerDistance: {
                const MobActor *mob = mobOf(*target);
                const ServerPlayer *mobOwner = mob == nullptr ? nullptr : mob->getOwner(owner);
                return mobOwner != nullptr && hasValue(filter)
                       && compareFloats(std::sqrt(mob->distanceSquaredTo(*mobOwner)), numberOf(filter),
                                        filter.mOperator);
            }
            case FilterTestType::InNether:
                return applyBoolean(target->getDimension() == DimensionType::Nether, filter);
            case FilterTestType::InOverworld:
                return applyBoolean(target->getDimension() == DimensionType::Overworld, filter);
            case FilterTestType::HasContainerOpen: {
                const ServerPlayer *player = playerOf(*target);
                return applyBoolean(player != nullptr && player->getInventoryManager().isContainerOpen(), filter);
            }
            case FilterTestType::HasSilkTouch: {
                const ServerPlayer *player = playerOf(*target);
                const bool result = player != nullptr
                                    && ItemEnchantments::getLevel(player->getInventory().getItemInHand(),
                                                                  EnchantmentIds::SILK_TOUCH)
                                               > 0;
                return applyBoolean(result, filter);
            }
            case FilterTestType::HasEquipment:
                return applyNegation(hasValue(filter)
                                             && hasEquipmentIn(*target, filter.mEquipmentDomain,
                                                               BehaviorItems(filter.mItems.get())),
                                     filter);
            case FilterTestType::HasEquipmentTag:
                return applyNegation(hasValue(filter)
                                             && hasEquipmentTagIn(*target, filter.mEquipmentDomain, filter.mText),
                                     filter);
            case FilterTestType::OnGround:
                return applyBoolean(target->isOnGround(), filter);
            case FilterTestType::OnFire:
                return applyBoolean(target->isOnFire(), filter);
            case FilterTestType::IsRiding:
                return applyBoolean(target->isRiding(), filter);
            case FilterTestType::IsRidingSelf:
                return applyBoolean(target->getVehicleId() == self.getUniqueId(), filter);
            case FilterTestType::IsSprinting:
                return applyBoolean(target->getFlags().get(ActorFlag::Sprinting), filter);
            case FilterTestType::IsMoving: {
                const Vector3f motion = target->getMotion();
                const bool moving = target->getFlags().get(ActorFlag::Moving)
                                    || motion.x * motion.x + motion.z * motion.z > 1.0e-6f;
                return applyBoolean(moving, filter);
            }
            case FilterTestType::IsSleeping: {
                const ServerPlayer *player = playerOf(*target);
                const bool sleeping = player != nullptr ? player->isSleeping()
                                                        : target->getFlags().get(ActorFlag::Sleeping);
                return applyBoolean(sleeping, filter);
            }
            case FilterTestType::IsLeashed:
                return applyBoolean(target->getFlags().get(ActorFlag::Leashed), filter);
            case FilterTestType::IsVisible:
                return applyBoolean(!target->hasEffect(MobEffectId::Invisibility), filter);
            case FilterTestType::IsMissingHealth:
                return applyBoolean(target->getHealth() < target->getMaxHealth(), filter);
            case FilterTestType::IsPersistent: {
                const ServerActor *actor = dynamic_cast<const ServerActor *>(target);
                return applyBoolean(actor != nullptr && actor->isPersistent(), filter);
            }
            case FilterTestType::IsTamed: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && mob->isTamed(), filter);
            }
            case FilterTestType::IsOwner: {
                const MobActor *selfMob = mobOf(self);
                return applyBoolean(selfMob != nullptr && selfMob->isOwnedBy(*target), filter);
            }
            case FilterTestType::WasLastHurtBy: {
                const MobActor *selfMob = mobOf(self);
                return applyBoolean(selfMob != nullptr && selfMob->getLastHurtBy() != 0
                                            && selfMob->getLastHurtBy() == target->getRuntimeId(),
                                    filter);
            }
            case FilterTestType::IsAvoidingMobs: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && mob->isAvoidingMobs(), filter);
            }
            case FilterTestType::IsNavigating: {
                const MobActor *mob = mobOf(*target);
                return applyBoolean(mob != nullptr && !mob->getNavigation().isDone(), filter);
            }
            case FilterTestType::InactivityTimer: {
                const MobActor *mob = mobOf(*target);
                const int32_t seconds = mob == nullptr ? 0 : mob->getInactivityTicks() / TICKS_PER_SECOND;
                // a bare == reads as at least
                return hasValue(filter)
                       && compareNumbers(seconds, wholeOf(filter),
                                         filter.mExactEquals ? FilterOperator::GreaterEqual : filter.mOperator);
            }
            case FilterTestType::HasAbility: {
                const ServerPlayer *player = playerOf(*target);
                return applyNegation(player != nullptr && hasValue(filter) && hasAbility(*player, filter.mText),
                                     filter);
            }
            case FilterTestType::HasMobEffect:
                return applyNegation(hasValue(filter) && filter.mHasEffect && target->hasEffect(filter.mEffect),
                                     filter);
            case FilterTestType::IsColor: {
                const int32_t color = componentValue(*target, "minecraft:color");
                return applyNegation(hasValue(filter) && color >= 0 && color < 16 && filter.mText == COLOR_NAMES[color],
                                     filter);
            }
            case FilterTestType::IsSkinId:
                return hasValue(filter)
                       && compareNumbers(componentValue(*target, "minecraft:skin_id"), wholeOf(filter),
                                         filter.mOperator);
            case FilterTestType::HasRangedWeapon: {
                const std::vector<const ItemStack *> hand = equipmentOf(*target, "hand");
                const std::string held = hand.empty() ? std::string() : itemIdentifier(*hand.front());
                const bool result = std::find(std::begin(RANGED_WEAPONS), std::end(RANGED_WEAPONS), held)
                                    != std::end(RANGED_WEAPONS);
                return applyBoolean(result, filter);
            }
            case FilterTestType::AllSlotsEmpty: {
                bool empty = true;
                for (const ItemStack *item: equipmentOf(*target, filter.mEquipmentDomain))
                    empty = empty && item->isAir();
                return applyNegation(empty, filter);
            }
            case FilterTestType::HasDamagedEquipment: {
                bool result = false;
                for (const ItemStack *item: equipmentOf(*target, filter.mEquipmentDomain))
                    result = result
                             || (item->mDamage > 0
                                 && (filter.mQualified.empty() || itemIdentifier(*item) == filter.mQualified));
                return applyNegation(result, filter);
            }
            case FilterTestType::HasItemWithEnchantmentInSlot: {
                const EnchantmentData *enchantment = hasValue(filter) ? EnchantmentTable::findByName(filter.mText)
                                                                      : nullptr;
                bool result = false;
                if (enchantment != nullptr) {
                    for (const ItemStack *item: equipmentOf(*target, filter.mEquipmentDomain))
                        result = result || ItemEnchantments::getLevel(*item, enchantment->mId) > 0;
                }
                return applyNegation(result, filter);
            }
            case FilterTestType::HasSameEquipmentInSlotAs: {
                const std::vector<const ItemStack *> mine = equipmentOf(self, filter.mEquipmentDomain);
                const std::vector<const ItemStack *> theirs = equipmentOf(*target, filter.mEquipmentDomain);
                const bool result = !mine.empty() && !theirs.empty() && !mine.front()->isAir()
                                    && itemIdentifier(*mine.front()) == itemIdentifier(*theirs.front());
                return applyBoolean(result, filter);
            }
            case FilterTestType::DistanceToNearestPlayer:
                return hasValue(filter)
                       && compareFloats(nearestPlayerDistance(owner, *target), numberOf(filter), filter.mOperator);
            case FilterTestType::InCaravan:
                return applyBoolean(FollowCaravanGoal::isInCaravan(target->getUniqueId()), filter);
            case FilterTestType::IsInVillage: {
                const std::shared_ptr<Village> village = owner.getLevelFor(*target)
                                                                 .getVillageManager()
                                                                 .getClosestVillage(blockPosition(*target), 0);
                return applyBoolean(village != nullptr, filter);
            }
            case FilterTestType::Unsupported:
                return applyBoolean(false, filter);
            default:
                return testAtBlock(filter, owner, *target);
        }
    }

    bool testNode(const FilterNode &node, ServerNetworkHandler &owner, const ServerActor &self, const Actor *other) {
        switch (node.mMode) {
            case FilterMode::All:
                for (const FilterNode &child: node.mChildren) {
                    if (!testNode(child, owner, self, other))
                        return false;
                }
                return true;
            case FilterMode::Any:
                for (const FilterNode &child: node.mChildren) {
                    if (testNode(child, owner, self, other))
                        return true;
                }
                return false;
            case FilterMode::None:
                for (const FilterNode &child: node.mChildren) {
                    if (testNode(child, owner, self, other))
                        return false;
                }
                return true;
            default:
                return testOne(node.mTest, owner, self, other);
        }
    }
}

bool EntityFilter::hasFamily(const Actor &actor, const std::string &family) {
    if (actor.isPlayer())
        return family == PLAYER_FAMILY;

    const json::Value *typeFamily = nullptr;
    if (const MobActor *mob = mobOf(actor)) {
        typeFamily = mob->getComponent(TYPE_FAMILY_COMPONENT);
    } else if (const ServerActor *serverActor = serverActorOf(actor)) {
        const json::Value *definition = EntityDefinitions::find(serverActor->getIdentifier());
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        typeFamily = components == nullptr ? nullptr : components->get(TYPE_FAMILY_COMPONENT);
    }

    const json::Value *families = typeFamily == nullptr ? nullptr : typeFamily->get("family");
    if (families == nullptr)
        return false;

    for (const std::unique_ptr<json::Value> &entry: families->mArray) {
        if (textOf(*entry) == family)
            return true;
    }
    return false;
}

EntityFilter::DamageScope::DamageScope(const ActorDamageSource &source, float amount) {
    activeDamage = &source;
    activeDamageAmount = amount;
}

EntityFilter::DamageScope::~DamageScope() {
    activeDamage = nullptr;
    activeDamageAmount = 0.0f;
}

std::shared_ptr<const EntityFilter::Group> EntityFilter::compile(const json::Value *filter) {
    return filter == nullptr ? nullptr : std::make_shared<const Group>(compileNode(*filter));
}

void EntityFilter::compileDefinitions(const json::Value *root) {
    compiledFilters().clear();
    if (root != nullptr)
        compileTree(*root);
}

bool EntityFilter::test(const Group &filter, ServerNetworkHandler &owner, const ServerActor &self,
                        const Actor *other) {
    return testNode(filter, owner, self, other);
}

// runs for every candidate of every scan: the filter was compiled with its definition
bool EntityFilter::test(const json::Value &filter, ServerNetworkHandler &owner, const ServerActor &self,
                        const Actor *other) {
    const std::unordered_map<const json::Value *, FilterNode> &filters = compiledFilters();
    const auto found = filters.find(&filter);
    if (found != filters.end())
        return testNode(found->second, owner, self, other);
    return testNode(compileNode(filter), owner, self, other);
}
