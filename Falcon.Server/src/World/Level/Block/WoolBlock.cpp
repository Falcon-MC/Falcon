#include "World/Level/Block/WoolBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Vibration/VibrationProperties.h"

FALCON_REGISTER_BLOCK(WoolBlock, 385);

bool WoolBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_wool");
}

// the 16 wool registrations pass dampen and occlude
uint8_t WoolBlock::getVibrationProperties() const {
    return VibrationProperties::DAMPENS | VibrationProperties::OCCLUDES;
}
