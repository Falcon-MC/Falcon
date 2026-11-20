#pragma once

#include "Server/Commands/CommandOrigin.h"

class PluginPermissions;

// what a function's commands run as: the caller seen at the call, kept while its commands wait on the
// stack; they print nothing, the function reports for them
class FunctionCommandOrigin final : public CommandOrigin {
public:
    // permissions null: only the permission level decides
    FunctionCommandOrigin(CommandOrigin &caller, const PluginPermissions *permissions)
            : mName(caller.getSenderName()), mIsPlayer(caller.isPlayer()), mPlayer(caller.asPlayer()),
              mPermissionPlayer(caller.permissionPlayer()), mGroupPlayer(caller.permissionGroupPlayer()),
              mPermissions(permissions), mLocale(caller.getLocale()),
              mPermission(caller.getCommandPermission()), mPosition(caller.getPosition()),
              mRotation(caller.getRotation()), mLevel(caller.getLevel()), mEntity(caller.getEntity()),
              mWithoutCheats(caller.canUseCommandsWithoutCheats()) {
        mHasBlock = caller.getSourceBlock(mBlockLevel, mBlock);
    }

    std::string getSenderName() const override {
        return mName;
    }

    bool isPlayer() const override {
        return mIsPlayer;
    }

    ServerPlayer *asPlayer() override {
        return mPlayer;
    }

    ServerPlayer *permissionPlayer() override {
        return mPermissionPlayer;
    }

    ServerPlayer *permissionGroupPlayer() override {
        return mGroupPlayer;
    }

    // the caller's own rule: a player's permission groups first, as a player origin decides
    bool canExecuteCommand(const Command &command) const override;

    void sendMessage(const std::string &) override {}

    void sendTranslation(const std::string &, const std::vector<std::string> &) override {}

    void sendError(const std::string &) override {}

    std::string getLocale() const override {
        return mLocale;
    }

    CommandPermission getCommandPermission() const override {
        return mPermission;
    }

    Vector3f getPosition() override {
        return mPosition;
    }

    Vector3f getRotation() override {
        return mRotation;
    }

    Level *getLevel() override {
        return mLevel;
    }

    Actor *getEntity() override {
        return mEntity;
    }

    bool getSourceBlock(Level *&level, Vector3i &position) override {
        level = mBlockLevel;
        position = mBlock;
        return mHasBlock;
    }

    bool canUseCommandsWithoutCheats() const override {
        return mWithoutCheats;
    }

private:
    std::string mName;
    bool mIsPlayer;
    ServerPlayer *mPlayer;
    ServerPlayer *mPermissionPlayer;
    ServerPlayer *mGroupPlayer;
    const PluginPermissions *mPermissions;
    std::string mLocale;
    CommandPermission mPermission;
    Vector3f mPosition;
    Vector3f mRotation;
    Level *mLevel;
    Actor *mEntity;
    bool mWithoutCheats;
    bool mHasBlock = false;
    Level *mBlockLevel = nullptr;
    Vector3i mBlock;
};
