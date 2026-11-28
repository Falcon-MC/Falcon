#pragma once

#include "World/Actor/ActorPropertySchema.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct BehaviorPack;

class EntityDefinitions {
public:
    enum class ComponentId : uint32_t {
        TypeFamily, Inventory, Strength, Movement, JumpStrength, FollowRange, IsChested, CollisionBox,
        Transformation, InstantDespawn, SpellEffects, Timer, BreakBlocks, DamageOverTime, Scheduler, AttackCooldown,
        Breathable, IsShaking, BurnsInDaylight, CanStandOnPowderSnow, Equippable, PushableByEntity, Count
    };

    struct NumericValue {
        std::optional<float> mValue;
        std::optional<float> mMinimum;
        std::optional<float> mMaximum;
        bool mRange = false;
    };

    struct Inventory {
        float mSize = 5.0f;
        float mSlotsPerStrength = 0.0f;
        bool mMount = false;
    };

    struct Equippable {
        size_t mSlots = 0;
    };

    struct Strength {
        float mValue = 0.0f;
        float mMaximum = 5.0f;
    };

    struct CollisionBox {
        std::optional<float> mWidth;
        std::optional<float> mHeight;
    };

    struct Component {
        uint32_t mId;
        const json::Value *mJson;
        std::variant<std::monostate, std::vector<std::string>, NumericValue, Inventory, Strength, CollisionBox,
                     Equippable> mFields;
    };

    using ComponentGroup = std::vector<Component>;

    struct CompiledDefinition {
        ComponentGroup mBase;
        std::map<std::string, ComponentGroup, std::less<>> mGroups;
        size_t mComponentSlots = 0;
    };

    static const CompiledDefinition *findCompiled(const json::Value *definition);

    static void initialize();

    static void loadPacks(const std::vector<BehaviorPack> &packs);

    static const json::Value *find(const std::string &identifier);

    static constexpr uint32_t NO_COMPONENT = UINT32_MAX;

    // a number per component name of the definitions, given when they load and kept for the process; NO_COMPONENT
    // for a name no definition has
    static uint32_t findComponentId(std::string_view name);

    static const std::string &getComponentName(uint32_t id);

    static const std::vector<ActorPropertyDescription> *findProperties(const std::string &identifier);

    static const std::map<std::string, std::vector<ActorPropertyDescription>> &getAllProperties();

private:
    static std::unique_ptr<json::Value> &_root();

    static std::map<std::string, std::vector<ActorPropertyDescription>> &_properties();
};
