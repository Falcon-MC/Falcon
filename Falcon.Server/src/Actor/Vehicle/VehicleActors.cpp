#include "Actor/ActorClassRegistry.h"
#include "Actor/SizedActor.h"

FALCON_REGISTER_SIZED_ACTOR(Boat, "minecraft:boat", 1.3f, 0.5f, false);
FALCON_REGISTER_SIZED_ACTOR(ChestBoat, "minecraft:chest_boat", 1.3f, 0.5f, false);
FALCON_REGISTER_SIZED_ACTOR(Minecart, "minecraft:minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(HopperMinecart, "minecraft:hopper_minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(TntMinecart, "minecraft:tnt_minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(ChestMinecart, "minecraft:chest_minecart", 0.98f, 0.7f, false);
FALCON_REGISTER_SIZED_ACTOR(CommandBlockMinecart, "minecraft:command_block_minecart", 0.98f, 0.7f, false);
