#include "Command/CameraCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Types/CameraPresets.h"

#include <cstdlib>
#include <string>
#include <unordered_map>

namespace {
    bool parseFloat(const std::string &value, float &out) {
        if (value.empty())
            return false;
        char *end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0')
            return false;
        out = (float) parsed;
        return true;
    }

    bool parseRelativeFloat(const std::string &value, float base, float &out) {
        if (!value.empty() && value[0] == '~') {
            if (value.size() == 1) {
                out = base;
                return true;
            }
            float offset = 0.0f;
            if (!parseFloat(value.substr(1), offset))
                return false;
            out = base + offset;
            return true;
        }
        return parseFloat(value, out);
    }

    bool parseCameraEase(const std::string &name, CameraEase &out) {
        static const std::unordered_map<std::string, CameraEase> eases = {
                {"linear", CameraEase::Linear},
                {"spring", CameraEase::Spring},
                {"in_sine", CameraEase::EaseInSine},
                {"out_sine", CameraEase::EaseOutSine},
                {"in_out_sine", CameraEase::EaseInOutSine},
                {"in_quad", CameraEase::EaseInQuad},
                {"out_quad", CameraEase::EaseOutQuad},
                {"in_out_quad", CameraEase::EaseInOutQuad},
                {"in_cubic", CameraEase::EaseInCubic},
                {"out_cubic", CameraEase::EaseOutCubic},
                {"in_out_cubic", CameraEase::EaseInOutCubic},
                {"in_quart", CameraEase::EaseInQuart},
                {"out_quart", CameraEase::EaseOutQuart},
                {"in_out_quart", CameraEase::EaseInOutQuart},
                {"in_quint", CameraEase::EaseInQuint},
                {"out_quint", CameraEase::EaseOutQuint},
                {"in_out_quint", CameraEase::EaseInOutQuint},
                {"in_expo", CameraEase::EaseInExpo},
                {"out_expo", CameraEase::EaseOutExpo},
                {"in_out_expo", CameraEase::EaseInOutExpo},
                {"in_circ", CameraEase::EaseInCirc},
                {"out_circ", CameraEase::EaseOutCirc},
                {"in_out_circ", CameraEase::EaseInOutCirc},
                {"in_back", CameraEase::EaseInBack},
                {"out_back", CameraEase::EaseOutBack},
                {"in_out_back", CameraEase::EaseInOutBack},
                {"in_elastic", CameraEase::EaseInElastic},
                {"out_elastic", CameraEase::EaseOutElastic},
                {"in_out_elastic", CameraEase::EaseInOutElastic},
                {"in_bounce", CameraEase::EaseInBounce},
                {"out_bounce", CameraEase::EaseOutBounce},
                {"in_out_bounce", CameraEase::EaseInOutBounce},
        };
        const auto it = eases.find(name);
        if (it == eases.end())
            return false;
        out = it->second;
        return true;
    }

    bool buildFadeInstruction(const std::vector<std::string> &arguments, size_t start,
                              CameraFadeInstruction &fade) {
        for (size_t index = start; index < arguments.size();) {
            const std::string &keyword = arguments[index];
            if (keyword == "time") {
                if (index + 3 >= arguments.size())
                    return false;
                if (!parseFloat(arguments[index + 1], fade.mTimeData.mFadeInTime) ||
                    !parseFloat(arguments[index + 2], fade.mTimeData.mWaitTime) ||
                    !parseFloat(arguments[index + 3], fade.mTimeData.mFadeOutTime))
                    return false;
                fade.mHasTimeData = true;
                index += 4;
            } else if (keyword == "color") {
                if (index + 3 >= arguments.size())
                    return false;
                float red = 0.0f, green = 0.0f, blue = 0.0f;
                if (!parseFloat(arguments[index + 1], red) || !parseFloat(arguments[index + 2], green) ||
                    !parseFloat(arguments[index + 3], blue))
                    return false;
                fade.mColor.mRed = red / 255.0f;
                fade.mColor.mGreen = green / 255.0f;
                fade.mColor.mBlue = blue / 255.0f;
                fade.mHasColor = true;
                index += 4;
            } else {
                return false;
            }
        }
        return true;
    }

    bool buildSetInstruction(const std::vector<std::string> &arguments, ServerPlayer &target,
                             CameraSetInstruction &set) {
        if (arguments.size() < 3)
            return false;

        const int presetId = cameraPresetRuntimeId(arguments[2]);
        if (presetId < 0)
            return false;
        set.mPresetRuntimeId = presetId;

        const Vector3f &position = target.getPosition();
        const Vector3f &rotation = target.getRotation();

        for (size_t index = 3; index < arguments.size();) {
            const std::string &keyword = arguments[index];
            if (keyword == "ease") {
                if (index + 2 >= arguments.size())
                    return false;
                float time = 0.0f;
                CameraEase easeType = CameraEase::Linear;
                if (!parseFloat(arguments[index + 1], time) || !parseCameraEase(arguments[index + 2], easeType))
                    return false;
                set.mEase.mHasValue = true;
                set.mEase.mTime = time;
                set.mEase.mEaseType = easeType;
                index += 3;
            } else if (keyword == "pos") {
                if (index + 3 >= arguments.size())
                    return false;
                if (!parseRelativeFloat(arguments[index + 1], position.x, set.mPos.x) ||
                    !parseRelativeFloat(arguments[index + 2], position.y, set.mPos.y) ||
                    !parseRelativeFloat(arguments[index + 3], position.z, set.mPos.z))
                    return false;
                set.mHasPos = true;
                index += 4;
            } else if (keyword == "rot") {
                if (index + 2 >= arguments.size())
                    return false;
                if (!parseRelativeFloat(arguments[index + 1], rotation.x, set.mRot.x) ||
                    !parseRelativeFloat(arguments[index + 2], rotation.y, set.mRot.y))
                    return false;
                set.mHasRot = true;
                index += 3;
            } else if (keyword == "default") {
                set.mHasDefaultPreset = true;
                set.mDefaultPreset = true;
                index += 1;
            } else {
                return false;
            }
        }
        return true;
    }
}

CameraCommand::CameraCommand(ServerNetworkHandler &handler)
        : Command("camera", "commands.camera.description",
                  "/camera <target> <clear|fade|set> ..."), mHandler(handler) {}

std::vector<CommandOverloadData> CameraCommand::getOverloads() const {
    CommandParamData mode;
    mode.mName = "mode";
    mode.mHasEnumData = true;
    mode.mEnumData.mName = "CameraMode";
    mode.mEnumData.mValues = {"clear", "fade", "set"};

    CommandParamData rest;
    rest.mName = "arguments";
    rest.mOptional = true;
    rest.mHasType = true;
    rest.mType = CommandParamType::String;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("target"));
    overload.mParameters.push_back(mode);
    overload.mParameters.push_back(rest);
    return {overload};
}

bool CameraCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.player.notFound", {});
        return false;
    }

    const std::string &mode = arguments[1];

    if (mode == "clear") {
        CameraInstructionPacket packet;
        packet.mHasClear = true;
        packet.mClear = true;
        for (ServerPlayer *target: targets)
            mHandler.getNetworkHandler().send(target->getNetworkIdentifier(), packet, mHandler.getCodecContext());
        return true;
    }

    if (mode == "fade") {
        CameraFadeInstruction fade;
        if (!buildFadeInstruction(arguments, 2, fade)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        CameraInstructionPacket packet;
        packet.mHasFadeInstruction = true;
        packet.mFadeInstruction = fade;
        for (ServerPlayer *target: targets)
            mHandler.getNetworkHandler().send(target->getNetworkIdentifier(), packet, mHandler.getCodecContext());
        return true;
    }

    if (mode == "set") {
        bool sentAny = false;
        for (ServerPlayer *target: targets) {
            CameraSetInstruction set;
            if (!buildSetInstruction(arguments, *target, set)) {
                sender.sendTranslation("commands.generic.usage", {getUsage()});
                return false;
            }
            CameraInstructionPacket packet;
            packet.mHasSetInstruction = true;
            packet.mSetInstruction = set;
            mHandler.getNetworkHandler().send(target->getNetworkIdentifier(), packet, mHandler.getCodecContext());
            sentAny = true;
        }
        return sentAny;
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}
