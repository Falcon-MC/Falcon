#pragma once

#include "Server/Commands/CommandOrigin.h"

class NpcActor;

// an npc dialogue's commands: the npc runs them, @initiator is the player who talked to it; nothing reaches chat
class NpcCommandOrigin final : public CommandOrigin {
public:
    NpcCommandOrigin(Level &level, NpcActor &npc, ServerPlayer &initiator)
            : mLevel(level), mNpc(npc), mInitiator(initiator) {}

    std::string getSenderName() const override;

    bool isPlayer() const override {
        return false;
    }

    ServerPlayer *asPlayer() override {
        return nullptr;
    }

    void sendMessage(const std::string &) override {}

    void sendTranslation(const std::string &, const std::vector<std::string> &) override {}

    void sendError(const std::string &) override {}

    CommandPermission getCommandPermission() const override {
        return CommandPermission::GameDirectors;
    }

    Vector3f getPosition() override;

    Vector3f getRotation() override;

    Level *getLevel() override {
        return &mLevel;
    }

    Actor *getEntity() override;

    ServerPlayer *getInitiator() override {
        return &mInitiator;
    }

    // only an operator in creative writes them, as for a command block
    bool canUseCommandsWithoutCheats() const override {
        return true;
    }

private:
    Level &mLevel;
    NpcActor &mNpc;
    ServerPlayer &mInitiator;
};
