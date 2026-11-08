#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class CartographyContainerScreenValidator : public ContainerScreenValidatorBase {
};

class CartographyContainerManagerModel : public ContainerManagerModel {
public:
    CartographyContainerManagerModel();

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
