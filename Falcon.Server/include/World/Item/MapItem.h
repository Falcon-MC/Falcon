#pragma once

#include "World/Item/Item.h"
#include "World/Level/Map/MapItemSavedData.h"

#include <cstdint>
#include <random>
#include <string>

class Level;
class ServerActor;
class World;

// the aux values a filled map takes (MapItem)
enum class MapItemType : int32_t {
    Normal = 0,
    Locator = 2,
    Locked = 6,
};

// MapItem: the data lives in the world's MapDataManager, the item only carries map_uuid
class MapItem : public Item {
public:
    static constexpr const char *IDENTIFIER = "minecraft:filled_map";
    static constexpr const char *TAG_MAP_ID = "map_uuid";
    static constexpr const char *TAG_DISPLAY_PLAYERS = "map_display_players";
    static constexpr const char *TAG_NAME_INDEX = "map_name_index";
    static constexpr const char *TAG_IS_INIT = "map_is_init";
    static constexpr const char *TAG_IS_SCALING = "map_is_scaling";
    static constexpr const char *TAG_REGENERATE = "map_regenerate";
    static constexpr const char *TAG_SCALE = "map_scale";

    explicit MapItem(const Item &base);

    static bool matches(const std::string &identifier);

    static bool isFilledMap(const ItemStack &item);

    // sub_141AE3940; -1 for an item without a map
    static int64_t getMapId(const ItemStack &item);

    // sub_141AE3A10: a locked map turns the item into the locked kind
    static void setMapId(ItemStack &item, const MapItemSavedData &data);

    static bool displaysPlayers(const ItemStack &item);

    // sub_141AE31C0; true when the item changed and its slot needs a resync
    static bool inventoryTick(ServerNetworkHandler &owner, ServerPlayer &player, ItemStack &item, bool selected);

    // MapItemSavedData::tickCarriedBy (sub_1483EF870)
    static void tickCarriedBy(ServerNetworkHandler &owner, World &world, MapItemSavedData &data, const ItemStack *item);

    // sub_1483EFCF0: refreshes every tracked decoration and drops those of actors gone
    static void updateTrackedDecorations(ServerNetworkHandler &owner, World &world, MapItemSavedData &data);

    // sub_1483F0C40; false when the tracked actor or block no longer shows on the map
    static bool updateTrackedDecoration(ServerNetworkHandler &owner, World &world, MapItemSavedData &data,
                                        MapItemTrackingInfo &tracking);

    // sub_1483F1560: whether the player carries a map with this id in the inventory or offhand
    static bool isHoldingMap(const ServerPlayer &player, int64_t mapId);

    // sub_142935230 for map positions: false between the End and another dimension
    static bool convertPosition(float &x, float &z, int32_t from, int32_t to);

    // the world level holding every map of the player's world
    static Level &getMapLevel(ServerNetworkHandler &owner, const Actor &actor);

    static std::mt19937 &random();
};
