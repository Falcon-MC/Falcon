#pragma once

#include <cstdint>

enum class ContainerType {
    None,
    Inventory,
    Container,
    Workbench,
    Furnace,
    Enchantment,
    BrewingStand,
    Anvil,
    Dispenser,
    Dropper,
    Hopper,
    Cauldron,
    MinecartChest,
    MinecartHopper,
    Horse,
    Beacon,
    StructureEditor,
    Trade,
    CommandBlock,
    Jukebox,
    Armor,
    Hand,
    CompoundCreator,
    ElementConstructor,
    MaterialReducer,
    LabTable,
    Loom,
    Lectern,
    Grindstone,
    BlastFurnace,
    Smoker,
    Stonecutter,
    Cartography,
    Hud,
    JigsawEditor,
    SmithingTable,
    ChestBoat,
    DecoratedPot,
    Crafter,
};

int32_t containerTypeToId(ContainerType type);

ContainerType containerTypeFromId(int32_t id);
