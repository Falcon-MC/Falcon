#pragma once

#include "Server/Commands/CommandOrigin.h"

class ServerNetworkHandler;

class ServerCommandOrigin : public CommandOrigin {
public:
    // below internal: pack content the server runs, not the console
    explicit ServerCommandOrigin(ServerNetworkHandler *handler = nullptr,
                                 CommandPermission permission = CommandPermission::Internal);

    std::string getSenderName() const override;

    bool isPlayer() const override {
        return false;
    }

    ServerPlayer *asPlayer() override {
        return nullptr;
    }

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    CommandPermission getCommandPermission() const override {
        return mPermission;
    }

    bool canManageServer() const override {
        return mPermission == CommandPermission::Internal;
    }

    Level *getLevel() override;

    bool canUseCommandsWithoutCheats() const override {
        return true;
    }

private:
    ServerNetworkHandler *mHandler;
    CommandPermission mPermission;
};
