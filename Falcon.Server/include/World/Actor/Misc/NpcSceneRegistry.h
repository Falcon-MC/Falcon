#pragma once

#include "World/Actor/Misc/NpcAction.h"

#include <string>
#include <unordered_map>
#include <vector>

// a scene of a behavior pack's dialogue folder: what an npc says and shows when /dialogue names it
struct NpcScene {
    std::string mTag;
    // plain text or a raw text json, as the file gives it
    std::string mNpcName;
    std::string mText;
    // the buttons first, then the opening and closing commands; a button press names its index here
    std::vector<NpcAction> mActions;
};

class NpcSceneRegistry {
public:
    static NpcSceneRegistry &getInstance();

    // nullptr for an unknown or empty tag
    const NpcScene *find(const std::string &tag) const;

    // the pack's dialogue folder; the first pack keeps a tag
    void addPack(const std::string &packRoot);

    // a minecraft:npc_dialogue file; false when it holds no scene
    bool addDefinition(const std::string &json);

    void clear() {
        mScenes.clear();
    }

private:
    NpcSceneRegistry() = default;

    std::unordered_map<std::string, NpcScene> mScenes;
};
