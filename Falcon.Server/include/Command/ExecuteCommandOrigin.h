#pragma once

#include "Command/CommandOrigin.h"

class Level;
class ServerPlayer;

class ExecuteCommandOrigin : public CommandOrigin {
public:
    ExecuteCommandOrigin(CommandOrigin &base, ServerPlayer *executor, const Vector3f &position,
                         const Vector3f &rotation, Level *level);

    std::string getSenderName() const override;

    bool isPlayer() const override { return mExecutor != nullptr; }

    ServerPlayer *asPlayer() override { return mExecutor; }

    void sendMessage(const std::string &message) override;

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override;

    std::string getLocale() const override;

    CommandPermission getCommandPermission() const override;

    Vector3f getPosition() override { return mPosition; }

    Vector3f getRotation() override { return mRotation; }

    Level *getLevel() override { return mLevel; }

    void setPosition(const Vector3f &position) { mPosition = position; }

    void setRotation(const Vector3f &rotation) { mRotation = rotation; }

    void setExecutor(ServerPlayer *executor) { mExecutor = executor; }

    void setLevel(Level *level) { mLevel = level; }

private:
    CommandOrigin &mBase;
    ServerPlayer *mExecutor;
    Vector3f mPosition;
    Vector3f mRotation;
    Level *mLevel;
};
