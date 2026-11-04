#ifndef FALCON_WASM_H
#define FALCON_WASM_H

/* plain callbacks over the wit-bindgen bindings; falcon_plugin.h stays usable for everything not wrapped */

#include "falcon_plugin.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t falcon_player;

typedef void (*falcon_event_handler)(void);

typedef bool (*falcon_command_handler)(int argc, const char *const *argv);

typedef void (*falcon_task_handler)(void);

/* implemented by the plugin: false refuses to enable */
bool falcon_enable(void);

/* optional */
void falcon_disable(void);

void falcon_log(const char *message);

void falcon_warn(const char *message);

void falcon_error(const char *message);

void falcon_broadcast(const char *message);

/* type: a FALCON_EVENT_* value; false when refused */
bool falcon_on(uint32_t event, falcon_event_handler handler);

/* false when the name is taken */
bool falcon_command(const char *name, const char *description, falcon_command_handler handler);

void falcon_reply(const char *message);

/* period 0: runs once after delay ticks */
bool falcon_schedule(uint64_t delay, uint64_t period, falcon_task_handler handler);

bool falcon_event_player(falcon_player *player);

bool falcon_sender_player(falcon_player *player);

void falcon_send(falcon_player player, const char *message);

/* truncated to fit */
const char *falcon_player_name(falcon_player player, char *buffer, uint32_t size);

#define FALCON_EVENT_PLAYER_JOIN 1u
#define FALCON_EVENT_PLAYER_QUIT 2u
#define FALCON_EVENT_PLAYER_CHAT 3u
#define FALCON_EVENT_BLOCK_BREAK 4u
#define FALCON_EVENT_BLOCK_PLACE 5u
#define FALCON_EVENT_PLAYER_INTERACT_BLOCK 6u
#define FALCON_EVENT_ENTITY_DAMAGE 7u
#define FALCON_EVENT_ENTITY_DEATH 8u
#define FALCON_EVENT_PLAYER_DEATH 9u
#define FALCON_EVENT_PLAYER_RESPAWN 10u
#define FALCON_EVENT_PLAYER_MOVE 11u
#define FALCON_EVENT_PLAYER_DROP_ITEM 12u
#define FALCON_EVENT_PLAYER_PICKUP_ITEM 13u
#define FALCON_EVENT_DATA_PACKET_RECEIVE 14u
#define FALCON_EVENT_DATA_PACKET_SEND 15u
#define FALCON_EVENT_PLAYER_INTERACT_ENTITY 16u
#define FALCON_EVENT_INVENTORY_OPEN 17u
#define FALCON_EVENT_INVENTORY_CLOSE 18u
#define FALCON_EVENT_INVENTORY_TRANSACTION 19u
#define FALCON_EVENT_CRAFT_ITEM 20u
#define FALCON_EVENT_PLAYER_COMMAND 21u
#define FALCON_EVENT_SERVER_COMMAND 22u
#define FALCON_EVENT_SERVER_TICK 23u
#define FALCON_EVENT_PLAYER_GAME_MODE_CHANGE 24u
#define FALCON_EVENT_PLAYER_CHANGE_DIMENSION 25u
#define FALCON_EVENT_PROJECTILE_HIT 26u
#define FALCON_EVENT_EXPLOSION 27u
#define FALCON_EVENT_FIRE_SPREAD 28u
#define FALCON_EVENT_BLOCK_BURN 29u
#define FALCON_EVENT_CUSTOM 30u
#define FALCON_EVENT_PLAYER_PRE_LOGIN 31u
#define FALCON_EVENT_PLAYER_TOGGLE_SNEAK 32u
#define FALCON_EVENT_PLAYER_TOGGLE_SPRINT 33u
#define FALCON_EVENT_PLAYER_TOGGLE_FLIGHT 34u
#define FALCON_EVENT_PLAYER_ITEM_HELD 35u
#define FALCON_EVENT_PLAYER_ITEM_CONSUME 36u
#define FALCON_EVENT_PLAYER_BED_ENTER 37u
#define FALCON_EVENT_PLAYER_BED_LEAVE 38u
#define FALCON_EVENT_PLAYER_JUMP 39u
#define FALCON_EVENT_PLAYER_FOOD_CHANGE 40u
#define FALCON_EVENT_PLAYER_EXPERIENCE_CHANGE 41u
#define FALCON_EVENT_ENTITY_SPAWN 42u
#define FALCON_EVENT_ENTITY_TRANSFORM 43u
#define FALCON_EVENT_ENTITY_TARGET 44u
#define FALCON_EVENT_PROJECTILE_LAUNCH 45u
#define FALCON_EVENT_BLOCK_GROW 46u
#define FALCON_EVENT_BLOCK_SPREAD 47u
#define FALCON_EVENT_BLOCK_FORM 48u
#define FALCON_EVENT_BLOCK_FADE 49u
#define FALCON_EVENT_LIQUID_FLOW 50u
#define FALCON_EVENT_PISTON_EXTEND 51u
#define FALCON_EVENT_PISTON_RETRACT 52u
#define FALCON_EVENT_REDSTONE_CHANGE 53u
#define FALCON_EVENT_SIGN_CHANGE 54u
#define FALCON_EVENT_LEAVES_DECAY 55u
#define FALCON_EVENT_WEATHER_CHANGE 56u
#define FALCON_EVENT_THUNDER_CHANGE 57u
#define FALCON_EVENT_CHUNK_LOAD 58u
#define FALCON_EVENT_CHUNK_UNLOAD 59u
#define FALCON_EVENT_FURNACE_SMELT 60u
#define FALCON_EVENT_FURNACE_BURN 61u
#define FALCON_EVENT_ITEM_ENCHANT 62u
#define FALCON_EVENT_ITEM_DAMAGE 63u
#define FALCON_EVENT_ITEM_BREAK 64u
#define FALCON_EVENT_WORLD_LOAD 65u
#define FALCON_EVENT_WORLD_UNLOAD 66u
#define FALCON_EVENT_PLAYER_CHANGE_WORLD 67u
#define FALCON_EVENT_CONNECTION_OPEN 68u
#define FALCON_EVENT_PLAYER_FORM_RESPONSE 69u
#define FALCON_GAME_MODE_SURVIVAL 0u
#define FALCON_GAME_MODE_CREATIVE 1u
#define FALCON_GAME_MODE_ADVENTURE 2u
#define FALCON_GAME_MODE_SPECTATOR 6u
#define FALCON_DIMENSION_OVERWORLD 0u
#define FALCON_DIMENSION_NETHER 1u
#define FALCON_DIMENSION_THE_END 2u

#endif
