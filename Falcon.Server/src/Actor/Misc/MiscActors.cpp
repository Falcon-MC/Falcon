#include "Actor/ActorClassRegistry.h"
#include "Actor/SizedActor.h"

FALCON_REGISTER_SIZED_ACTOR(Npc, "minecraft:npc", 0.6f, 2.1f, false);
FALCON_REGISTER_SIZED_ACTOR(ArmorStand, "minecraft:armor_stand", 0.5f, 1.975f, false);
FALCON_REGISTER_SIZED_ACTOR(ItemEntity, "minecraft:item", 0.25f, 0.25f, false);
FALCON_REGISTER_SIZED_ACTOR(PrimedTnt, "minecraft:tnt", 0.98f, 0.98f, false);
FALCON_REGISTER_SIZED_ACTOR(FallingBlock, "minecraft:falling_block", 0.98f, 0.98f, false);
FALCON_REGISTER_SIZED_ACTOR(AreaEffectCloud, "minecraft:area_effect_cloud", 3.0f, 0.5f, false);
