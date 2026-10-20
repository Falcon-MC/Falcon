#include "World/Level/Block/CauldronBlock.h"

#include <algorithm>

int CauldronBlock::getFillLevel() const {
    return std::clamp(getStates().getInt("fill_level", 0), 0, 6);
}

CauldronBlock::Liquid CauldronBlock::getLiquid() const {
    if (getFillLevel() == 0)
        return Liquid::Empty;
    return getStates().getString("cauldron_liquid", "water") == "lava" ? Liquid::Lava : Liquid::Water;
}

bool CauldronBlock::isFull() const {
    return getFillLevel() == 6;
}

bool CauldronBlock::canFill(Liquid liquid) const {
    if (liquid == Liquid::Empty)
        return false;
    if (getFillLevel() == 6)
        return false;
    return getFillLevel() == 0 || getLiquid() == liquid;
}

BlockState CauldronBlock::withFillLevel(Liquid liquid, int fillLevel) const {
    return makeState(liquid, fillLevel);
}

BlockState CauldronBlock::makeState(Liquid liquid, int fillLevel) {
    Tag states = Tag::ofCompound();
    states.putString("cauldron_liquid", liquid == Liquid::Lava ? "lava" : "water");
    states.putInt("fill_level", std::clamp(fillLevel, 0, 6));
    return BlockState("minecraft:cauldron", states);
}
