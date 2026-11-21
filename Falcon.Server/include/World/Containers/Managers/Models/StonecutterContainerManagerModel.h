#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class StonecutterContainerScreenValidator : public ContainerScreenValidatorBase {};

class StonecutterContainerManagerModel : public ContainerManagerModel {
public:
    StonecutterContainerManagerModel();

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
