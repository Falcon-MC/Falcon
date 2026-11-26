#include "World/Level/Block/Systems/CircuitComponents.h"

#include "World/Level/Block/Circuit/CircuitSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"

std::optional<int> CircuitComponents::strengthAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    (void) owner;
    const BaseCircuitComponent *component = RedstoneSystem::circuitOf(level).findComponent(position);
    if (component == nullptr || component->getBaseType() == CircuitComponentType::PoweredBlock)
        return std::nullopt;
    return component->getStrength();
}
