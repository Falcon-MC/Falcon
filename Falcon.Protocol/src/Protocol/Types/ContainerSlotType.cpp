#include "Protocol/Types/ContainerSlotType.h"

namespace {
    const ContainerSlotType TYPES_BY_ID[] = {
        ContainerSlotType::AnvilInput,
        ContainerSlotType::AnvilMaterial,
        ContainerSlotType::AnvilResult,
        ContainerSlotType::SmithingTableInput,
        ContainerSlotType::SmithingTableMaterial,
        ContainerSlotType::SmithingTableResult,
        ContainerSlotType::Armor,
        ContainerSlotType::LevelEntity,
        ContainerSlotType::BeaconPayment,
        ContainerSlotType::BrewingInput,
        ContainerSlotType::BrewingResult,
        ContainerSlotType::BrewingFuel,
        ContainerSlotType::HotbarAndInventory,
        ContainerSlotType::CraftingInput,
        ContainerSlotType::CraftingOutput,
        ContainerSlotType::RecipeConstruction,
        ContainerSlotType::RecipeNature,
        ContainerSlotType::RecipeItems,
        ContainerSlotType::RecipeSearch,
        ContainerSlotType::RecipeSearchBar,
        ContainerSlotType::RecipeEquipment,
        ContainerSlotType::RecipeBook,
        ContainerSlotType::EnchantingInput,
        ContainerSlotType::EnchantingMaterial,
        ContainerSlotType::FurnaceFuel,
        ContainerSlotType::FurnaceIngredient,
        ContainerSlotType::FurnaceResult,
        ContainerSlotType::HorseEquip,
        ContainerSlotType::Hotbar,
        ContainerSlotType::Inventory,
        ContainerSlotType::ShulkerBox,
        ContainerSlotType::TradeIngredient1,
        ContainerSlotType::TradeIngredient2,
        ContainerSlotType::TradeResult,
        ContainerSlotType::Offhand,
        ContainerSlotType::CompoundCreatorInput,
        ContainerSlotType::CompoundCreatorOutput,
        ContainerSlotType::ElementConstructorOutput,
        ContainerSlotType::MaterialReducerInput,
        ContainerSlotType::MaterialReducerOutput,
        ContainerSlotType::LabTableInput,
        ContainerSlotType::LoomInput,
        ContainerSlotType::LoomDye,
        ContainerSlotType::LoomMaterial,
        ContainerSlotType::LoomResult,
        ContainerSlotType::BlastFurnaceIngredient,
        ContainerSlotType::SmokerIngredient,
        ContainerSlotType::Trade2Ingredient1,
        ContainerSlotType::Trade2Ingredient2,
        ContainerSlotType::Trade2Result,
        ContainerSlotType::GrindstoneInput,
        ContainerSlotType::GrindstoneAdditional,
        ContainerSlotType::GrindstoneResult,
        ContainerSlotType::StonecutterInput,
        ContainerSlotType::StonecutterResult,
        ContainerSlotType::CartographyInput,
        ContainerSlotType::CartographyAdditional,
        ContainerSlotType::CartographyResult,
        ContainerSlotType::Barrel,
        ContainerSlotType::Cursor,
        ContainerSlotType::CreatedOutput,
        ContainerSlotType::SmithingTableTemplate,
        ContainerSlotType::CrafterBlockContainer,
        ContainerSlotType::DynamicContainer,
        ContainerSlotType::RecipeFoodContainer,
        ContainerSlotType::RecipeBlocksContainer,
        ContainerSlotType::RecipeFurnaceItemsContainer,
        ContainerSlotType::ReservedA,
        ContainerSlotType::ReservedB,
        ContainerSlotType::ReservedC,
        ContainerSlotType::ReservedD,
    };

    const int32_t TYPE_COUNT = sizeof(TYPES_BY_ID) / sizeof(TYPES_BY_ID[0]);
}

int32_t containerSlotTypeToId(ContainerSlotType type) {
    for (int32_t i = 0; i < TYPE_COUNT; i++) {
        if (TYPES_BY_ID[i] == type) {
            return i;
        }
    }
    return -1;
}

ContainerSlotType containerSlotTypeFromId(int32_t id) {
    if (id < 0 || id >= TYPE_COUNT) {
        return ContainerSlotType::Unknown;
    }
    return TYPES_BY_ID[id];
}
