#include "World/Item/ShearsItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(ShearsItem, 130);

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/CobwebBlock.h"
#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"
#include "World/Level/Block/LeavesBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/VineBlock.h"

namespace {
    const float WEB_AND_LEAVES_SPEED = 15.0f;
    const float WOOL_SPEED = 5.0f;
    const float VINE_SPEED = 2.0f;
}

ShearsItem::ShearsItem(const Item &base) : Item(base) {}

bool ShearsItem::matches(const std::string &identifier) {
    return identifier == "minecraft:shears";
}

bool ShearsItem::getDestroySpeed(const std::string &blockIdentifier, float &speed) const {
    const Block *block = VanillaBlocks::fromIdentifier(blockIdentifier);
    if (dynamic_cast<const CobwebBlock *>(block) != nullptr || dynamic_cast<const LeavesBlock *>(block) != nullptr
        || dynamic_cast<const FloweredAzaleaLeavesBlock *>(block) != nullptr) {
        speed = WEB_AND_LEAVES_SPEED;
        return true;
    }

    // Besides wool, the data table only assigns shears to plants that break instantly.
    const BlockData *data = BlockDataTable::find(blockIdentifier.c_str());
    if (data != nullptr && data->mToolType == BlockToolType::Shears) {
        speed = WOOL_SPEED;
        return true;
    }

    // Glow lichen has no block class of its own; the client lists it next to vines.
    if (dynamic_cast<const VineBlock *>(block) != nullptr || blockIdentifier == "minecraft:glow_lichen") {
        speed = VINE_SPEED;
        return true;
    }

    (void) speed;
    return false;
}

bool ShearsItem::canDestroySpecial(const std::string &blockIdentifier) const {
    return dynamic_cast<const CobwebBlock *>(VanillaBlocks::fromIdentifier(blockIdentifier)) != nullptr;
}
