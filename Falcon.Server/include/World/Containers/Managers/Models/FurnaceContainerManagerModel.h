#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Containers/Managers/Models/ContainerManagerModel.h"

class FurnaceContainerScreenValidator : public ContainerScreenValidatorBase {
};

class FurnaceContainerManagerModel : public ContainerManagerModel {
public:
    explicit FurnaceContainerManagerModel(const BlockState &state);

protected:
    bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) override;

private:
    BlockState mState;
};
