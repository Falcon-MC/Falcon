#include "Command/ExecuteCommandOrigin.h"

#include "Actor/ServerPlayer.h"

ExecuteCommandOrigin::ExecuteCommandOrigin(CommandOrigin &base, ServerPlayer *executor, const Vector3f &position,
                                           const Vector3f &rotation, Level *level)
        : mBase(base), mExecutor(executor), mPosition(position), mRotation(rotation), mLevel(level) {}

const std::string &ExecuteCommandOrigin::getSenderName() const {
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

CommandPermission ExecuteCommandOrigin::getCommandPermission() const {
    return mBase.getCommandPermission();
}
