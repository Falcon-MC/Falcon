#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class AnvilContainerScreenValidator : public ContainerScreenValidatorBase {};

class AnvilContainerManagerModel : public ContainerManagerModel {
public:
    AnvilContainerManagerModel();

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
