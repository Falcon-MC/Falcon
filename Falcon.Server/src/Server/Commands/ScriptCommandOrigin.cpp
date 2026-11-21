#include "Server/Commands/ScriptCommandOrigin.h"

#include "Server/Localization.h"
#include "World/Actor/Player/ServerPlayer.h"

ScriptCommandOrigin::ScriptCommandOrigin(Level &level, Actor *entity) : mLevel(level), mEntity(entity) {}

std::string ScriptCommandOrigin::getSenderName() const {
    // commands.origin.script once translated
    return mEntity != nullptr ? mEntity->getName()
                              : Localization::getInstance().translate(getLocale(), "commands.origin.script", {});
}

bool ScriptCommandOrigin::isPlayer() const {
    return dynamic_cast<const ServerPlayer *>(mEntity) != nullptr;
}

ServerPlayer *ScriptCommandOrigin::asPlayer() {
    return dynamic_cast<ServerPlayer *>(mEntity);
}

void ScriptCommandOrigin::sendMessage(const std::string &message) {
    mLastKey.clear();
    mLastMessage = message;
    ++mOutputCount;
}

void ScriptCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    mLastKey = key;
    if (key.compare(0, 17, "commands.generic.") != 0)
        ++mOutputCount;
    mLastMessage = Localization::getInstance().translate(getLocale(), key, parameters);
}

Vector3f ScriptCommandOrigin::getPosition() {
    // a dimension's script origin sits at the world origin
    return mEntity != nullptr ? mEntity->getPosition() : Vector3f(0.0f, 0.0f, 0.0f);
}

Vector3f ScriptCommandOrigin::getRotation() {
    return mEntity != nullptr ? mEntity->getRotation() : Vector3f(0.0f, 0.0f, 0.0f);
}
