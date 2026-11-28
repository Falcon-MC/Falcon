#include "World/Actor/Definition/EntityDefinitions.h"

#include "Core/Debug/BedrockLog.h"
#include "Scripting/BehaviorPackManager.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "EntityDefinitionsJson.h"

#include <algorithm>
#include <array>
#include <deque>
#include <unordered_map>

namespace {
    using ComponentId = EntityDefinitions::ComponentId;
    constexpr std::array BUILTIN_NAMES{"minecraft:type_family", "minecraft:inventory", "minecraft:strength",
                                      "minecraft:movement", "minecraft:horse.jump_strength", "minecraft:follow_range",
                                      "minecraft:is_chested", "minecraft:collision_box",
                                      "minecraft:transformation", "minecraft:instant_despawn", "minecraft:spell_effects",
                                      "minecraft:timer", "minecraft:break_blocks", "minecraft:damage_over_time",
                                      "minecraft:scheduler", "minecraft:attack_cooldown", "minecraft:breathable",
                                      "minecraft:is_shaking", "minecraft:burns_in_daylight",
                                      "minecraft:can_stand_on_powder_snow", "minecraft:equippable",
                                      "minecraft:pushable_by_entity"};
    static_assert(BUILTIN_NAMES.size() == (size_t) ComponentId::Count);

    struct ComponentIds {
        ComponentIds() {
            for (const char *name: BUILTIN_NAMES)
                intern(name);
        }

        uint32_t intern(std::string_view name) {
            if (const auto found = mIds.find(name); found != mIds.end())
                return found->second;
            mNames.emplace_back(name);
            const auto id = (uint32_t) (mNames.size() - 1);
            mIds.emplace(mNames.back(), id);
            return id;
        }
        // a deque keeps every name in place: the keys of the index point into it
        std::deque<std::string> mNames;
        std::unordered_map<std::string_view, uint32_t> mIds;
    };

    ComponentIds &componentIds() {
        static ComponentIds ids;
        return ids;
    }

    std::map<std::string, std::unique_ptr<json::Value>, std::less<>> packDefinitions;

    using CompiledDefinitions = std::unordered_map<const json::Value *, EntityDefinitions::CompiledDefinition>;
    CompiledDefinitions vanillaComponents;
    CompiledDefinitions packComponents;

    std::optional<float> number(const json::Value *value) {
        return value != nullptr && value->isNumber() ? std::optional<float>((float) value->number()) : std::nullopt;
    }

    EntityDefinitions::Component compileComponent(uint32_t id, const json::Value &data) {
        EntityDefinitions::Component result{id, &data, {}};
        switch ((ComponentId) id) {
            case ComponentId::TypeFamily: {
                auto &families = result.mFields.emplace<std::vector<std::string>>();
                if (const auto *list = data.get("family")) {
                    families.reserve(list->mArray.size());
                    for (const auto &value: list->mArray)
                        families.push_back(value->string());
                }
                break;
            }
            case ComponentId::Inventory: {
                const auto *type = data.get("container_type");
                EntityDefinitions::Inventory inventory;
                inventory.mSize = number(data.get("inventory_size")).value_or(inventory.mSize);
                inventory.mSlotsPerStrength =
                        number(data.get("additional_slots_per_strength")).value_or(inventory.mSlotsPerStrength);
                inventory.mMount = type != nullptr && type->string() == "horse";
                result.mFields = inventory;
                break;
            }
            case ComponentId::Equippable: {
                const auto *slots = data.get("slots");
                result.mFields = EntityDefinitions::Equippable{
                    slots != nullptr && slots->isArray() ? slots->mArray.size() : 0};
                break;
            }
            case ComponentId::Strength:
                result.mFields = EntityDefinitions::Strength{number(data.get("value")).value_or(0.0f),
                    number(data.get("max")).value_or(5.0f)};
                break;
            case ComponentId::CollisionBox:
                result.mFields = EntityDefinitions::CollisionBox{number(data.get("width")), number(data.get("height"))};
                break;
            case ComponentId::Movement:
            case ComponentId::JumpStrength:
            case ComponentId::FollowRange: {
                const auto *value = data.get("value");
                auto &fields = result.mFields.emplace<EntityDefinitions::NumericValue>();
                fields.mRange = value != nullptr && value->isObject();
                if (fields.mRange) {
                    fields.mMinimum = number(value->get("range_min"));
                    fields.mMaximum = number(value->get("range_max"));
                } else {
                    fields.mValue = number(value);
                }
                break;
            }
            default:
                break;
        }
        return result;
    }

    EntityDefinitions::CompiledDefinition compileDefinition(const json::Value &definition) {
        EntityDefinitions::CompiledDefinition result;
        const auto compileGroup = [&](const json::Value *data, EntityDefinitions::ComponentGroup &out) {
            if (data == nullptr)
                return;
            out.reserve(data->mKeys.size());
            for (const auto &name: data->mKeys) {
                const uint32_t id = componentIds().intern(name);
                out.push_back(compileComponent(id, *data->mObject.at(name)));
                result.mComponentSlots = std::max(result.mComponentSlots, (size_t) id + 1);
            }
        };
        compileGroup(definition.get("components"), result.mBase);
        if (const auto *groups = definition.get("component_groups")) {
            for (const auto &group: groups->mKeys)
                compileGroup(groups->get(group), result.mGroups[group]);
        }
        return result;
    }
}

std::unique_ptr<json::Value> &EntityDefinitions::_root() {
    static std::unique_ptr<json::Value> root;
    return root;
}

std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::_properties() {
    static std::map<std::string, std::vector<ActorPropertyDescription>> properties;
    return properties;
}

void EntityDefinitions::initialize() {
    // the compiled filters point into the definitions they come from
    EntityFilter::compileDefinitions(nullptr);
    vanillaComponents.clear();
    _root() = json::parse(FalconEntityData::kEntityDefinitionsJson);
    _properties().clear();
    if (_root() == nullptr || !_root()->isObject()) {
        _root().reset();
        LOG_WARN(LogAreaID::Server, "Could not parse the embedded entity definitions");
        return;
    }

    for (const std::string &identifier: _root()->mKeys) {
        const json::Value *definition = _root()->get(identifier);
        vanillaComponents.emplace(definition, compileDefinition(*definition));

        const json::Value *description = definition->get("description");
        const json::Value *properties = description == nullptr ? nullptr : description->get("properties");
        if (properties == nullptr)
            continue;

        std::vector<ActorPropertyDescription> schema = ActorPropertySchema::parse(*properties);
        if (!schema.empty())
            _properties()[identifier] = std::move(schema);
    }
    EntityFilter::compileDefinitions(_root().get());
}

void EntityDefinitions::loadPacks(const std::vector<BehaviorPack> &packs) {
    EntityFilter::clearPackDefinitions();
    packComponents.clear();
    packDefinitions.clear();
    for (const auto &pack: packs) {
        pack.visitJson("entities", [](const json::Value &doc) {
            const auto *definition = doc.get("minecraft:entity");
            const auto *description = definition == nullptr ? nullptr : definition->get("description");
            const auto *identifier = description == nullptr ? nullptr : description->get("identifier");
            if (identifier == nullptr || !identifier->isString() || identifier->mString.empty())
                return;

            packDefinitions[identifier->mString] = definition->clone();
        });
    }
    for (const auto &[identifier, definition]: packDefinitions) {
        packComponents.emplace(definition.get(), compileDefinition(*definition));
        EntityFilter::compilePackDefinition(*definition);
    }
}

const EntityDefinitions::CompiledDefinition *EntityDefinitions::findCompiled(const json::Value *definition) {
    if (const auto pack = packComponents.find(definition); pack != packComponents.end())
        return &pack->second;
    const auto vanilla = vanillaComponents.find(definition);
    return vanilla == vanillaComponents.end() ? nullptr : &vanilla->second;
}

const json::Value *EntityDefinitions::find(const std::string &identifier) {
    if (const auto pack = packDefinitions.find(identifier); pack != packDefinitions.end())
        return pack->second.get();
    return _root() == nullptr ? nullptr : _root()->get(identifier);
}

uint32_t EntityDefinitions::findComponentId(std::string_view name) {
    const ComponentIds &ids = componentIds();
    const auto found = ids.mIds.find(name);
    return found == ids.mIds.end() ? NO_COMPONENT : found->second;
}

const std::string &EntityDefinitions::getComponentName(uint32_t id) {
    return componentIds().mNames[id];
}

const std::vector<ActorPropertyDescription> *EntityDefinitions::findProperties(const std::string &identifier) {
    const auto found = _properties().find(identifier);
    return found == _properties().end() ? nullptr : &found->second;
}

const std::map<std::string, std::vector<ActorPropertyDescription>> &EntityDefinitions::getAllProperties() {
    return _properties();
}
