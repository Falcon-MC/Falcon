#include "World/Level/Vibration/VibrationProperties.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/VanillaBlocks.h"

namespace VibrationProperties {
    uint8_t of(const BlockState &state) {
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        if (custom != nullptr)
            return (uint8_t) ((custom->mCanDampenVibrations ? DAMPENS : 0)
                              | (custom->mCanOccludeVibrations ? OCCLUDES : 0));

        const Block *block = VanillaBlocks::fromState(state);
        return block == nullptr ? 0 : block->getVibrationProperties();
    }
}
