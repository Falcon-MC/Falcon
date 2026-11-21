#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/SizedActor.h"

FALCON_REGISTER_SIZED_ACTOR(ItemEntity, "minecraft:item", 0.25f, 0.25f, false);
FALCON_REGISTER_SIZED_ACTOR(PrimedTnt, "minecraft:tnt", 0.98f, 0.98f, false);
FALCON_REGISTER_SIZED_ACTOR(FallingBlock, "minecraft:falling_block", 0.98f, 0.98f, false);
namespace {
    // the cloud's age, radius and potion live in the server's lingering cloud table, not in the actor: a
    // reloaded cloud would stay forever without them
    class AreaEffectCloudActor : public SizedActor {
    public:
        AreaEffectCloudActor(uint64_t runtimeId, const std::string &identifier)
                : SizedActor(runtimeId, identifier, ActorSize{3.0f, 0.5f}, false) {}

        bool shouldSave() const override {
            return false;
        }
    };
}

FALCON_REGISTER_ACTOR(AreaEffectCloudActor, "minecraft:area_effect_cloud");
