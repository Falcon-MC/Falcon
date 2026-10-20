#pragma once

class CraftingGrid {
public:
    static const int SMALL_WIDTH = 2;
    static const int TABLE_WIDTH = 3;

    static int getSize(int width);

    static bool isValidWidth(int width);
};
