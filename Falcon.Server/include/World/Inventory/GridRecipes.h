#pragma once

#include "Protocol/Types/ItemStack.h"
#include "World/Inventory/CartographyRecipes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// the crafting grid's other recipes the client only knows by uuid: fireworks, book and map copies, repairs and
// decorated pots; the client works the result out, the server makes it again from the grid
class GridRecipes {
public:
    enum class Kind { None, Firework, BookCloning, MapCloning, MapExtending, MapUpgrading, Repair, DecoratedPot };

    // what the grid makes and how much of each slot one craft takes
    struct Craft {
        ItemStack mResult = ItemStack::air();
        std::vector<int32_t> mRequiredBySlot;
    };

    // a new stack of an item, air for an unknown one
    using Make = std::function<ItemStack(const std::string &identifier, int32_t count)>;

    // in the order the crafting data sends them
    static const std::vector<std::pair<Kind, const char *>> &uuids();

    static void setNetworkId(Kind kind, int32_t networkId);

    static Kind kindOf(int32_t networkId);

    // a map recipe without maps fails; the others don't need them
    static bool craft(Kind kind, const std::vector<ItemStack> &grid, const Make &make,
                      const CartographyRecipes::Maps *maps, Craft &craft);

    // gunpowder and paper make three rockets carrying the stars; gunpowder and dyes make a star; dyes on a star
    // give it fade colours
    static bool firework(const std::vector<ItemStack> &grid, const Make &make, Craft &craft);

    // copies of a written book, one per book and quill, a generation further; the original stays
    static bool bookCloning(const std::vector<ItemStack> &grid, Craft &craft);

    // a filled map and empty maps: as many filled maps of it
    static bool mapCloning(const std::vector<ItemStack> &grid, Craft &craft);

    // a filled map in the middle of eight paper: the map one scale out
    static bool mapExtending(const std::vector<ItemStack> &grid, const CartographyRecipes::Maps &maps, Craft &craft);

    // a map and a compass: a map that shows the players
    static bool mapUpgrading(const std::vector<ItemStack> &grid, Craft &craft);

    // two worn items of one kind: a new one with both durabilities and a twentieth more, keeping only curses
    static bool repair(const std::vector<ItemStack> &grid, const Make &make, Craft &craft);

    // four sherds or bricks in a cross: a pot with their faces
    static bool decoratedPot(const std::vector<ItemStack> &grid, const Make &make, Craft &craft);

    // the colour a firework star shows in the inventory, the average of its dyes
    static int32_t starColor(const std::vector<int8_t> &colors);

    // the explosion shape each item gives a star
    static constexpr int8_t SHAPE_SMALL = 0;
    static constexpr int8_t SHAPE_LARGE = 1;
    static constexpr int8_t SHAPE_STAR = 2;
    static constexpr int8_t SHAPE_CREEPER = 3;
    static constexpr int8_t SHAPE_BURST = 4;
    static constexpr int32_t ROCKETS = 3;
};
