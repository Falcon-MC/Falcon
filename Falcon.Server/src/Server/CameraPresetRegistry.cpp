#include "Server/CameraPresetRegistry.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Protocol/Types/CameraPresets.h"
#include "Scripting/BehaviorPackManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <sstream>

namespace {
    std::vector<CameraPreset> &presets() {
        static std::vector<CameraPreset> entries = standardCameraPresets();
        return entries;
    }

    bool number(const json::Value *value, float &out) {
        if (value == nullptr || !value->isNumber() || !std::isfinite(value->mNumber)
            || std::abs(value->mNumber) > std::numeric_limits<float>::max())
            return false;
        out = (float) value->mNumber;
        return true;
    }

    bool flag(const json::Value *value, bool &out) {
        if (value == nullptr || value->mType != json::Value::Type::Boolean)
            return false;
        out = value->mBoolean;
        return true;
    }

    bool vector(const json::Value *value, std::initializer_list<float *> components) {
        if (value == nullptr || !value->isArray() || value->mArray.size() != components.size())
            return false;
        size_t index = 0;
        for (float *component: components) {
            if (!number(value->mArray[index++].get(), *component))
                return false;
        }
        return true;
    }

    CameraPreset readPreset(const json::Value &body, const std::string &identifier, const std::string &parent) {
        CameraPreset preset;
        preset.mIdentifier = identifier;
        preset.mParentPreset = parent;
        preset.mHasPosX = number(body.get("pos_x"), preset.mPos.x);
        preset.mHasPosY = number(body.get("pos_y"), preset.mPos.y);
        preset.mHasPosZ = number(body.get("pos_z"), preset.mPos.z);
        preset.mHasPitch = number(body.get("rot_x"), preset.mPitch);
        preset.mHasYaw = number(body.get("rot_y"), preset.mYaw);
        preset.mHasRotationSpeed = number(body.get("rotation_speed"), preset.mRotationSpeed);
        preset.mHasSnapToTarget = flag(body.get("snap_to_target"), preset.mSnapToTarget);
        preset.mHasContinueTargeting = flag(body.get("continue_targeting"), preset.mContinueTargeting);
        preset.mHasBlockListeningRadius = number(body.get("tracking_radius"), preset.mBlockListeningRadius);
        preset.mHasHorizontalRotationLimit = vector(body.get("horizontal_rotation_limit"),
                                                     {&preset.mHorizontalRotationLimit.x, &preset.mHorizontalRotationLimit.y});
        preset.mHasVerticalRotationLimit = vector(body.get("vertical_rotation_limit"),
                                                   {&preset.mVerticalRotationLimit.x, &preset.mVerticalRotationLimit.y});
        preset.mHasViewOffset = vector(body.get("view_offset"), {&preset.mViewOffset.x, &preset.mViewOffset.y});
        preset.mHasActorOffset = vector(body.get("entity_offset"),
                                         {&preset.mActorOffset.x, &preset.mActorOffset.y, &preset.mActorOffset.z});
        preset.mHasRadius = number(body.get("radius"), preset.mRadius);
        preset.mHasStartingRotation = vector(body.get("starting_rotation"),
                                               {&preset.mStartingRotation.x, &preset.mStartingRotation.y});
        preset.mHasPlayEffect = flag(body.get("player_effects"), preset.mPlayEffect);

        if (const json::Value *listener = body.get("listener")) {
            const std::string value = listener->string();
            preset.mHasListener = value == "camera" || value == "player";
            if (value == "player")
                preset.mListener = CameraAudioListener::Player;
        }
        if (const json::Value *scheme = body.get("control_scheme")) {
            static const std::map<std::string, ControlScheme> schemes = {
                    {"locked_player_relative_strafe", ControlScheme::LockedPlayerRelativeStrafe},
                    {"camera_relative", ControlScheme::CameraRelative},
                    {"camera_relative_strafe", ControlScheme::CameraRelativeStrafe},
                    {"player_relative", ControlScheme::PlayerRelative},
                    {"player_relative_strafe", ControlScheme::PlayerRelativeStrafe},
            };
            const auto found = schemes.find(scheme->string());
            if (found != schemes.end()) {
                preset.mHasControlScheme = true;
                preset.mControlScheme = found->second;
            }
        }
        return preset;
    }
}

void CameraPresetRegistry::load(const std::vector<BehaviorPack> &packs) {
    namespace fs = std::filesystem;
    auto &loaded = presets();
    loaded = standardCameraPresets();
    std::map<std::string, CameraPreset> pending;
    for (const BehaviorPack &pack: packs) {
        const fs::path root = fs::path(pack.mPath) / "cameras" / "presets";
        std::error_code error;
        std::vector<fs::path> paths;
        for (fs::recursive_directory_iterator entry(root, error), end; !error && entry != end; entry.increment(error)) {
            if (entry->is_regular_file(error) && entry->path().extension() == ".json")
                paths.push_back(entry->path());
        }
        std::sort(paths.begin(), paths.end());
        for (const fs::path &path: paths) {
            std::ifstream file(path, std::ios::binary);
            std::stringstream contents;
            contents << file.rdbuf();
            const auto document = json::parse(contents.str());
            const json::Value *body = document != nullptr ? document->get("minecraft:camera_preset") : nullptr;
            const json::Value *identifier = body != nullptr ? body->get("identifier") : nullptr;
            const json::Value *parent = body != nullptr ? body->get("inherit_from") : nullptr;
            if (identifier == nullptr || !identifier->isString() || identifier->mString.empty()
                || parent == nullptr || !parent->isString() || parent->mString.empty()
                || runtimeId(identifier->mString) >= 0) {
                LOG_WARN(LogAreaID::Server, "Invalid camera preset in %s", path.string().c_str());
                continue;
            }
            pending.insert_or_assign(identifier->mString, readPreset(*body, identifier->mString, parent->mString));
        }
    }

    if (!pending.empty()) {
        for (const char *identifier: {"minecraft:follow_orbit", "minecraft:fixed_boom"}) {
            CameraPreset builtin;
            builtin.mIdentifier = identifier;
            loaded.push_back(std::move(builtin));
            pending.erase(identifier);
        }
    }

    // the packet defines a parent before any preset that inherits from it
    bool advanced = true;
    while (advanced && !pending.empty()) {
        advanced = false;
        for (auto entry = pending.begin(); entry != pending.end();) {
            if (runtimeId(entry->second.mParentPreset) < 0) {
                ++entry;
                continue;
            }
            loaded.push_back(std::move(entry->second));
            entry = pending.erase(entry);
            advanced = true;
        }
    }
    for (const auto &[identifier, preset]: pending)
        LOG_WARN(LogAreaID::Server, "Camera preset %s has unresolved parent %s", identifier.c_str(),
                 preset.mParentPreset.c_str());
}

const std::vector<CameraPreset> &CameraPresetRegistry::getPresets() {
    return presets();
}

int CameraPresetRegistry::runtimeId(const std::string &identifier) {
    const auto &entries = presets();
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const CameraPreset &preset) {
        return preset.mIdentifier == identifier;
    });
    return found == entries.end() ? -1 : (int) (found - entries.begin());
}
