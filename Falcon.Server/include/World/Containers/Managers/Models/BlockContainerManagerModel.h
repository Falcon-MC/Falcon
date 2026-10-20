#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class BlockContainerScreenValidator : public ContainerScreenValidatorBase {
};

class BlockContainerManagerModel : public ContainerManagerModel {
public:
    explicit BlockContainerManagerModel(ContainerType type);

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
