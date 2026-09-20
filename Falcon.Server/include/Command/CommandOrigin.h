#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Types/AdventureSettingData.h"

#include <string>
#include <vector>

class Level;
class ServerPlayer;

class CommandOrigin {
public:
    virtual ~CommandOrigin() = default;

    virtual const std::string &getSenderName() const = 0;

    virtual bool isPlayer() const = 0;

    virtual ServerPlayer *asPlayer() = 0;

    virtual void sendMessage(const std::string &message) = 0;

    virtual void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) = 0;

    virtual CommandPermission getCommandPermission() const = 0;

    virtual Vector3f getPosition();

    virtual Vector3f getRotation();

    virtual Level *getLevel();
};
