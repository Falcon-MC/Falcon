#pragma once

#include "Server/Commands/CommandOrigin.h"

#include <cstdint>
#include <string>
#include <vector>

// Entity.runCommand and Dimension.runCommand: output stays with the caller, nothing reaches chat
class ScriptCommandOrigin : public CommandOrigin {
public:
    // entity null: the dimension runs it from its origin
    ScriptCommandOrigin(Level &level, Actor *entity);

    std::string getSenderName() const override;

    bool isPlayer() const override;

    ServerPlayer *asPlayer() override;

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    CommandPermission getCommandPermission() const override {
        // sub_14C34A800, sub_14B59EF00: the script origin is built with level 1, whoever the entity is
        return CommandPermission::GameDirectors;
    }

    Vector3f getPosition() override;

    Vector3f getRotation() override;

    Level *getLevel() override {
        return &mLevel;
    }

    Actor *getEntity() override {
        return mEntity;
    }

    bool canUseCommandsWithoutCheats() const override {
        return true;
    }

    // the last translated key the command sent, empty when it sent none
    const std::string &getLastKey() const {
        return mLastKey;
    }

    const std::string &getLastMessage() const {
        return mLastMessage;
    }

    // outputs other than commands.generic errors: one per target the command acted on
    int32_t getOutputCount() const {
        return mOutputCount;
    }

private:
    Level &mLevel;
    Actor *mEntity;
    std::string mLastKey;
    std::string mLastMessage;
    int32_t mOutputCount = 0;
};
