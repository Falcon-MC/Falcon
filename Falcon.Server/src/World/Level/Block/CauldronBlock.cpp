#include "World/Level/Block/CauldronBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(CauldronBlock, 200);

bool CauldronBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cauldron";
}

int CauldronBlock::getFillLevel() const {
    return std::clamp(getStates().getInt("fill_level", 0), 0, 6);
}

CauldronBlock::Liquid CauldronBlock::getLiquid() const {
    if (getFillLevel() == 0)
        return Liquid::Empty;
    const std::string liquid = getStates().getString("cauldron_liquid", "water");
    if (liquid == "lava")
        return Liquid::Lava;
    return liquid == "powder_snow" ? Liquid::PowderSnow : Liquid::Water;
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
    states.putString("cauldron_liquid",
                     liquid == Liquid::Lava ? "lava" : liquid == Liquid::PowderSnow ? "powder_snow" : "water");
    states.putInt("fill_level", std::clamp(fillLevel, 0, 6));
    return BlockState("minecraft:cauldron", states);
}
