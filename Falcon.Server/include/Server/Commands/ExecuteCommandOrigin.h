#pragma once

#include "Server/Commands/CommandOrigin.h"

class Actor;
class Level;
class ServerPlayer;

class ExecuteCommandOrigin : public CommandOrigin {
public:
    // the executor is a player or any other entity (execute as @e)
    ExecuteCommandOrigin(CommandOrigin &base, Actor *executor, const Vector3f &position, const Vector3f &rotation,
                         Level *level);

    std::string getSenderName() const override;

    bool isPlayer() const override;

    ServerPlayer *asPlayer() override;

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    std::string getLocale() const override;

    CommandPermission getCommandPermission() const override;

    Vector3f getPosition() override {
        return mPosition;
    }

    Vector3f getRotation() override {
        return mRotation;
    }

    Level *getLevel() override {
        return mLevel;
    }

    // without an executor, the base origin's entity keeps running it
    Actor *getEntity() override;

    bool getSourceBlock(Level *&level, Vector3i &position) override {
        return mBase.getSourceBlock(level, position);
    }

    bool canUseCommandsWithoutCheats() const override {
        return mBase.canUseCommandsWithoutCheats();
    }

    void sendError(const std::string &message) override {
        mBase.sendError(message);
    }

    void setPosition(const Vector3f &position) {
        mPosition = position;
    }

    void setRotation(const Vector3f &rotation) {
        mRotation = rotation;
    }

    void setExecutor(Actor *executor) {
        mExecutor = executor;
    }

    void setLevel(Level *level) {
        mLevel = level;
    }

private:
    CommandOrigin &mBase;
    Actor *mExecutor;
    Vector3f mPosition;
    Vector3f mRotation;
    Level *mLevel;
};
