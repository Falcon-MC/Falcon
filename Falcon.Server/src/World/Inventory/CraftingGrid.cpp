#include "World/Inventory/CraftingGrid.h"

int CraftingGrid::getSize(int width) {
    return isValidWidth(width) ? width * width : 0;
}

bool CraftingGrid::isValidWidth(int width) {
    return width == SMALL_WIDTH || width == TABLE_WIDTH;
}
