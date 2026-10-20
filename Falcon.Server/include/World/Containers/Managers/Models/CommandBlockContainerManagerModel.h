#pragma once

#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class CommandBlockContainerScreenValidator : public ContainerScreenValidatorBase {
public:
    bool isValid(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) const override;
};

class CommandBlockContainerManagerModel : public ContainerManagerModel {
public:
    CommandBlockContainerManagerModel();

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;
};
