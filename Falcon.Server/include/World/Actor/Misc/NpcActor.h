#pragma once

#include "World/Actor/Misc/NpcAction.h"
#include "World/Actor/ServerActor.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ServerNetworkHandler;

// the actor behind the minecraft:npc component: its skins come from npc_data.skin_list of the definition
class NpcActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:npc";

    // the client opens the npc screen only for an actor flagged with the component
    static constexpr int32_t HAS_NPC_DATA_ID = 39;

    // the component's npc_data json: the portrait and picker offsets and the skin list
    static constexpr int32_t NPC_DATA_DATA_ID = 40;

    static constexpr int32_t ACTIONS_DATA_ID = 41;

    // the dialogue text
    static constexpr int32_t INTERACT_TEXT_DATA_ID = 100;

    // the raw text name the client shows in the npc screen
    static constexpr int32_t NAME_RAW_TEXT_DATA_ID = 112;

    // the name tag keeps 32 code points at most
    static constexpr size_t MAX_NAME_TAG_LENGTH = 32;

    struct Skin {
        std::optional<int32_t> mVariant;
        std::optional<int32_t> mMarkVariant;
    };

    NpcActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override {
        return ActorSize{0.6f, 2.1f};
    }

    const std::vector<Skin> &getSkins() const {
        return mSkins;
    }

    // -1 when the index points past the skin list, like the script getter
    int32_t getSkinIndex() const;

    // false on a negative index, one past the list or the current one
    bool setSkinIndex(ServerNetworkHandler *owner, int32_t index);

    const std::string &getRawName() const {
        return mRawName;
    }

    // keeps the raw text and names the actor with its resolved text
    void setName(ServerNetworkHandler *owner, const std::string &rawName);

    const std::string &getSceneName() const {
        return mSceneName;
    }

    void setSceneName(const std::string &sceneName) {
        mSceneName = sceneName;
    }

    // the scene /dialogue change gave this player, else the npc's own scene
    const std::string &getSceneFor(int64_t playerUniqueId) const;

    // empty: back to the npc's own scene
    void setPlayerScene(int64_t playerUniqueId, const std::string &sceneName) {
        if (sceneName.empty())
            mPlayerScenes.erase(playerUniqueId);
        else
            mPlayerScenes[playerUniqueId] = sceneName;
    }

    // the scene the player's open dialogue shows; its buttons are the only scene ones the player may press
    const std::string &getOpenedScene(int64_t playerUniqueId) const;

    void setOpenedScene(int64_t playerUniqueId, const std::string &sceneName) {
        mOpenedScenes[playerUniqueId] = sceneName;
    }

    const std::vector<NpcAction> &getActions() const {
        return mActions;
    }

    // false when nothing changed
    bool setActions(ServerNetworkHandler *owner, std::vector<NpcAction> actions);

    const std::string &getInteractText() const {
        return mInteractText;
    }

    void setInteractText(ServerNetworkHandler *owner, const std::string &text);

    // the npc's dialogue: an operator in creative gets the editor
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    // the plain text a raw text json resolves to; anything else stays as it is
    static std::string resolveName(const std::string &rawName);

private:
    void _appendSkin(EntityDataMap &metadata) const;

    void _sendString(ServerNetworkHandler *owner, int32_t id, const std::string &value);

    std::vector<Skin> mSkins;
    std::string mNpcData;
    std::vector<NpcAction> mActions;
    std::string mInteractText;
    std::unordered_map<int64_t, std::string> mPlayerScenes;
    std::unordered_map<int64_t, std::string> mOpenedScenes;
    int32_t mSkinIndex = 0;
    std::string mRawName;
    std::string mSceneName;
};
