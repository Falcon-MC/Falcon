#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/AdventureSettingData.h"

#include <string>
#include <vector>

class Actor;
class Level;
class ServerPlayer;

class CommandOrigin {
public:
    virtual ~CommandOrigin() = default;

    virtual std::string getSenderName() const = 0;

    virtual bool isPlayer() const = 0;

    virtual ServerPlayer *asPlayer() = 0;

    virtual void sendMessage(const std::string &message) = 0;

    virtual void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) = 0;

    virtual std::string getLocale() const;

    void sendLocalized(const std::string &key, const std::vector<std::string> &parameters = {});

    virtual CommandPermission getCommandPermission() const = 0;

    virtual Vector3f getPosition();

    virtual Vector3f getRotation();

    virtual Level *getLevel();

    // the entity running the command, a player or not; null for the server and blocks
    virtual Actor *getEntity();

    // command blocks: false for any other origin
    virtual bool getSourceBlock(Level *&level, Vector3i &position);

    // sub_14576E1A0: the dedicated server, scripts, automation and tests run cheat commands without cheats
    virtual bool canUseCommandsWithoutCheats() const {
        return false;
    }

    // error output: the client shows it red
    virtual void sendError(const std::string &message);
};
