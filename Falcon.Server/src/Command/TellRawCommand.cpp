#include "Command/TellRawCommand.h"

#include "Actor/ServerPlayer.h"
#include "Core/Json/Json.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/TextPacket.h"

namespace {
    bool serializeRawText(const json::Value &root, std::string &out, std::string &errorKey);

    bool serializeComponent(const json::Value &component, std::string &out, std::string &errorKey) {
        if (!component.isObject()) {
            errorKey = "commands.tellraw.error.itemIsNotObject";
            return false;
        }

        std::string serialized;

        const json::Value *text = component.get("text");
        if (text != nullptr) {
            if (!text->isString()) {
                errorKey = "commands.tellraw.error.textNotString";
                return false;
            }
            serialized += "{\"text\":\"" + json::escape(text->mString) + "\"}";
            out += serialized;
            return true;
        }

        const json::Value *translate = component.get("translate");
        if (translate == nullptr) {
            errorKey = "commands.tellraw.error.itemIsNotObject";
            return false;
        }
        if (!translate->isString()) {
            errorKey = "commands.tellraw.error.translateNotString";
            return false;
        }

        serialized += "{\"translate\":\"" + json::escape(translate->mString) + "\"";

        const json::Value *with = component.get("with");
        if (with != nullptr) {
            if (with->isArray()) {
                serialized += ",\"with\":[";
                for (size_t index = 0; index < with->mArray.size(); ++index) {
                    const json::Value &entry = *with->mArray[index];
                    if (!entry.isString()) {
                        errorKey = "commands.tellraw.error.withNotArrayOrRawText";
                        return false;
                    }
                    if (index != 0)
                        serialized += ",";
                    serialized += "\"" + json::escape(entry.mString) + "\"";
                }
                serialized += "]";
            } else if (with->isObject()) {
                std::string nested;
                if (!serializeRawText(*with, nested, errorKey))
                    return false;
                serialized += ",\"with\":" + nested;
            } else {
                errorKey = "commands.tellraw.error.withNotArrayOrRawText";
                return false;
            }
        }

        serialized += "}";
        out += serialized;
        return true;
    }

    bool serializeRawText(const json::Value &root, std::string &out, std::string &errorKey) {
        if (!root.isObject()) {
            errorKey = "commands.tellraw.error.notArray";
            return false;
        }

        const json::Value *rawText = root.get("rawtext");
        if (rawText == nullptr || !rawText->isArray()) {
            errorKey = "commands.tellraw.error.notArray";
            return false;
        }

        std::string serialized = "{\"rawtext\":[";
        for (size_t index = 0; index < rawText->mArray.size(); ++index) {
            if (index != 0)
                serialized += ",";
            if (!serializeComponent(*rawText->mArray[index], serialized, errorKey))
                return false;
        }
        serialized += "]}";

        out = serialized;
        return true;
    }
}

TellRawCommand::TellRawCommand(ServerNetworkHandler &handler)
        : Command("tellraw", "commands.tellraw.description", "/tellraw <target> <raw json message>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> TellRawCommand::getOverloads() const {
    CommandParamData jsonParameter;
    jsonParameter.mName = "raw json message";
    jsonParameter.mHasType = true;
    jsonParameter.mType = CommandParamType::Json;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("target"));
    overload.mParameters.push_back(jsonParameter);

    return {overload};
}

bool TellRawCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string source = joinArguments(arguments, 1);
    if (source.empty()) {
        sender.sendTranslation("commands.tellraw.error.noData", {});
        return false;
    }

    const std::unique_ptr<json::Value> root = json::parse(source);
    if (root == nullptr) {
        sender.sendTranslation("commands.tellraw.jsonStringException", {});
        return false;
    }

    std::string serialized;
    std::string errorKey;
    if (!serializeRawText(*root, serialized, errorKey)) {
        sender.sendTranslation(errorKey, {});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    TextPacket packet;
    packet.mType = TextPacket::Type::Json;
    packet.mNeedsTranslation = true;
    packet.mMessage = serialized;

    for (ServerPlayer *target: targets)
        mHandler.sendPacketTo(target->getNetworkIdentifier(), packet);

    return true;
}
