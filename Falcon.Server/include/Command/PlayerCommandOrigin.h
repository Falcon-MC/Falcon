#pragma once

#include "Command/CommandOrigin.h"
#include "Protocol/Types/CommandOriginData.h"

class ServerNetworkHandler;
class ServerPlayer;

class PlayerCommandOrigin : public CommandOrigin {
public:
    PlayerCommandOrigin(ServerNetworkHandler &handler, ServerPlayer &player, const CommandOriginData &origin);

    const std::string &getSenderName() const override;

    bool isPlayer() const override { return true; }

    ServerPlayer *asPlayer() override { return &mPlayer; }

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    CommandPermission getCommandPermission() const override;

    Level *getLevel() override;

private:
    ServerNetworkHandler &mHandler;
    ServerPlayer &mPlayer;
    CommandOriginData mOrigin;
};
