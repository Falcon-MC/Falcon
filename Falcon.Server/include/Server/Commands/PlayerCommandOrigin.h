#pragma once

#include "Server/Commands/CommandOrigin.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Types/CommandOriginData.h"

#include <vector>

class ServerNetworkHandler;
class ServerPlayer;

class PlayerCommandOrigin : public CommandOrigin {
public:
    PlayerCommandOrigin(ServerNetworkHandler &handler, ServerPlayer &player, const CommandOriginData &origin);

    std::string getSenderName() const override;

    bool isPlayer() const override {
        return true;
    }

    ServerPlayer *asPlayer() override {
        return &mPlayer;
    }

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    std::string getLocale() const override;

    void sendError(const std::string &message) override;

    CommandPermission getCommandPermission() const override;

    bool canExecuteCommand(const Command &command) const override;

    // the permission level still decides: only an operator gets past it
    bool canManageServer() const override {
        return true;
    }

    ServerPlayer *permissionGroupPlayer() override {
        return &mPlayer;
    }

    Level *getLevel() override;

    const CommandOriginData &getOriginData() const {
        return mOrigin;
    }

    // from here the command's messages wait for its result, then go out as one command output, failed or not:
    // the client shows a failed command's output as an error
    void collectOutput() {
        mCollecting = true;
    }

    void sendCollectedOutput(bool success);

private:
    struct CollectedMessage {
        CommandOutputMessage mMessage;
        bool mError = false;
    };

    void _collect(std::string text, std::vector<std::string> parameters, bool error);

    bool mCollecting = false;
    std::vector<CollectedMessage> mCollected;

    ServerNetworkHandler &mHandler;
    ServerPlayer &mPlayer;
    CommandOriginData mOrigin;
};
