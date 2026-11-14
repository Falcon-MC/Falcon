#include "Server/Commands/ExecuteCommandOrigin.h"

#include "World/Actor/Player/ServerPlayer.h"

ExecuteCommandOrigin::ExecuteCommandOrigin(CommandOrigin &base, Actor *executor, const Vector3f &position,
                                           const Vector3f &rotation, Level *level)
        : mBase(base), mExecutor(executor), mPosition(position), mRotation(rotation), mLevel(level) {}

bool ExecuteCommandOrigin::isPlayer() const {
    return dynamic_cast<const ServerPlayer *>(mExecutor) != nullptr;
}

ServerPlayer *ExecuteCommandOrigin::asPlayer() {
    return dynamic_cast<ServerPlayer *>(mExecutor);
}

std::string ExecuteCommandOrigin::getSenderName() const {
    if (mExecutor != nullptr)
        return mExecutor->getName();

    return mBase.getSenderName();
}

void ExecuteCommandOrigin::sendMessage(const std::string &message) {
    mBase.sendMessage(message);
}

void ExecuteCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    mBase.sendTranslation(key, parameters);
}

std::string ExecuteCommandOrigin::getLocale() const {
    return mBase.getLocale();
}

CommandPermission ExecuteCommandOrigin::getCommandPermission() const {
    return mBase.getCommandPermission();
}

Actor *ExecuteCommandOrigin::getEntity() {
    if (mExecutor != nullptr)
        return mExecutor;
    return mBase.getEntity();
}
