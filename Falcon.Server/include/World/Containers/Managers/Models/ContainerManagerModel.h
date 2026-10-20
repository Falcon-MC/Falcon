#pragma once

#include "World/Containers/Managers/Models/ContainerScreenValidatorBase.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ContainerType.h"

#include <memory>

class ServerNetworkHandler;
class ServerPlayer;

class ContainerManagerModel {
public:
    ContainerManagerModel(ContainerType type, std::unique_ptr<ContainerScreenValidatorBase> validator);

    virtual ~ContainerManagerModel() = default;

    ContainerType getType() const { return mType; }

    bool open(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

protected:
    virtual bool openWindow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

protected:
    ContainerType mType;
    std::unique_ptr<ContainerScreenValidatorBase> mValidator;
};
