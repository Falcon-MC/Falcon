#pragma once

#include "Command/CommandOrigin.h"

class ServerNetworkHandler;

class ServerCommandOrigin : public CommandOrigin {
public:
    explicit ServerCommandOrigin(ServerNetworkHandler *handler = nullptr);

    const std::string &getSenderName() const override;

    bool isPlayer() const override { return false; }

    ServerPlayer *asPlayer() override { return nullptr; }

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    CommandPermission getCommandPermission() const override { return CommandPermission::Internal; }

    Level *getLevel() override;

private:
    ServerNetworkHandler *mHandler;
};
