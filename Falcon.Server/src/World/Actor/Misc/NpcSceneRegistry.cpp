#include "World/Actor/Misc/NpcSceneRegistry.h"

#include "Core/Json/Json.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    // a raw text object goes to the client as its json
    std::string textOf(const json::Value *value) {
        if (value == nullptr)
            return std::string();
        if (value->isString())
            return value->mString;
        return value->isObject() ? json::stringify(*value) : std::string();
    }

    std::vector<NpcAction::Command> commandsOf(const json::Value *list) {
        std::vector<NpcAction::Command> commands;
        if (list == nullptr || !list->isArray())
            return commands;
        for (const std::unique_ptr<json::Value> &line: list->mArray) {
            if (line->isString() && !line->mString.empty())
                commands.push_back({line->mString, NpcAction::COMMAND_VERSION});
        }
        return commands;
    }

    NpcAction commandAction(NpcAction::Mode mode, const std::string &name, std::vector<NpcAction::Command> commands) {
        NpcAction action;
        action.mType = NpcAction::Type::Command;
        action.mMode = mode;
        action.mButtonName = name;
        for (const NpcAction::Command &command: commands)
            action.mText += (action.mText.empty() ? "" : "\n") + command.mLine;
        action.mCommands = std::move(commands);
        return action;
    }
}

NpcSceneRegistry &NpcSceneRegistry::getInstance() {
    static NpcSceneRegistry instance;
    return instance;
}

const NpcScene *NpcSceneRegistry::find(const std::string &tag) const {
    if (tag.empty())
        return nullptr;
    const auto scene = mScenes.find(tag);
    return scene == mScenes.end() ? nullptr : &scene->second;
}

void NpcSceneRegistry::addPack(const std::string &packRoot) {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(packRoot) / "dialogue";
    std::error_code error;
    if (!fs::is_directory(root, error))
        return;
    for (fs::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (it->is_regular_file(error) && it->path().extension() == ".json")
            addDefinition(readFile(it->path()));
    }
}

bool NpcSceneRegistry::addDefinition(const std::string &json) {
    const std::unique_ptr<json::Value> definition = json::parse(json);
    const json::Value *dialogue = definition == nullptr ? nullptr : definition->get("minecraft:npc_dialogue");
    const json::Value *scenes = dialogue == nullptr ? nullptr : dialogue->get("scenes");
    if (scenes == nullptr || !scenes->isArray())
        return false;
    bool added = false;
    for (const std::unique_ptr<json::Value> &entry: scenes->mArray) {
        const json::Value *tag = entry->get("scene_tag");
        if (tag == nullptr || !tag->isString() || tag->mString.empty() || mScenes.count(tag->mString) != 0)
            continue;
        NpcScene scene;
        scene.mTag = tag->mString;
        scene.mNpcName = textOf(entry->get("npc_name"));
        scene.mText = textOf(entry->get("text"));
        const json::Value *buttons = entry->get("buttons");
        if (buttons != nullptr && buttons->isArray()) {
            for (const std::unique_ptr<json::Value> &button: buttons->mArray) {
                const std::string name = textOf(button->get("name"));
                const json::Value *url = button->get("url");
                if (url != nullptr && url->isString()) {
                    NpcAction action;
                    action.mType = NpcAction::Type::Url;
                    action.mButtonName = name;
                    action.mText = url->mString;
                    scene.mActions.push_back(std::move(action));
                    continue;
                }
                scene.mActions.push_back(
                        commandAction(NpcAction::Mode::Button, name, commandsOf(button->get("commands"))));
            }
        }
        std::vector<NpcAction::Command> opening = commandsOf(entry->get("on_open_commands"));
        if (!opening.empty())
            scene.mActions.push_back(commandAction(NpcAction::Mode::OnOpen, std::string(), std::move(opening)));
        std::vector<NpcAction::Command> closing = commandsOf(entry->get("on_close_commands"));
        if (!closing.empty())
            scene.mActions.push_back(commandAction(NpcAction::Mode::OnClose, std::string(), std::move(closing)));
        mScenes.emplace(scene.mTag, std::move(scene));
        added = true;
    }
    return added;
}
