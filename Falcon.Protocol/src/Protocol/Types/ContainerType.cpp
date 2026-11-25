#include "Protocol/Types/ContainerType.h"

namespace {
    struct ContainerTypeEntry {
        ContainerType mType;
        int32_t mId;
    };

    const ContainerTypeEntry TYPE_ENTRIES[] = {
        {ContainerType::None, -9},
        {ContainerType::Inventory, -1},
        {ContainerType::Container, 0},
        {ContainerType::Workbench, 1},
        {ContainerType::Furnace, 2},
        {ContainerType::Enchantment, 3},
        {ContainerType::BrewingStand, 4},
        {ContainerType::Anvil, 5},
        {ContainerType::Dispenser, 6},
        {ContainerType::Dropper, 7},
        {ContainerType::Hopper, 8},
        {ContainerType::Cauldron, 9},
        {ContainerType::MinecartChest, 10},
        {ContainerType::MinecartHopper, 11},
        {ContainerType::Horse, 12},
        {ContainerType::Beacon, 13},
        {ContainerType::StructureEditor, 14},
        {ContainerType::Trade, 15},
        {ContainerType::CommandBlock, 16},
        {ContainerType::Jukebox, 17},
        {ContainerType::Armor, 18},
        {ContainerType::Hand, 19},
        {ContainerType::CompoundCreator, 20},
        {ContainerType::ElementConstructor, 21},
        {ContainerType::MaterialReducer, 22},
        {ContainerType::LabTable, 23},
        {ContainerType::Loom, 24},
        {ContainerType::Lectern, 25},
        {ContainerType::Grindstone, 26},
        {ContainerType::BlastFurnace, 27},
        {ContainerType::Smoker, 28},
        {ContainerType::Stonecutter, 29},
        {ContainerType::Cartography, 30},
        {ContainerType::Hud, 31},
        {ContainerType::JigsawEditor, 32},
        {ContainerType::SmithingTable, 33},
        {ContainerType::ChestBoat, 34},
        {ContainerType::DecoratedPot, 35},
        {ContainerType::Crafter, 36},
    };

    const int32_t TYPE_ENTRY_COUNT = sizeof(TYPE_ENTRIES) / sizeof(TYPE_ENTRIES[0]);
}

int32_t containerTypeToId(ContainerType type) {
    for (int32_t i = 0; i < TYPE_ENTRY_COUNT; i++) {
        if (TYPE_ENTRIES[i].mType == type) {
            return TYPE_ENTRIES[i].mId;
        }
    }
    return -9;
}

ContainerType containerTypeFromId(int32_t id) {
    for (int32_t i = 0; i < TYPE_ENTRY_COUNT; i++) {
        if (TYPE_ENTRIES[i].mId == id) {
            return TYPE_ENTRIES[i].mType;
        }
    }
    return ContainerType::None;
}
