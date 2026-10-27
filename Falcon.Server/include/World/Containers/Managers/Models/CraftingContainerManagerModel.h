#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class CraftingContainerScreenValidator : public ContainerScreenValidatorBase {
};

class CraftingContainerManagerModel : public ContainerManagerModel {
public:
    CraftingContainerManagerModel();

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
