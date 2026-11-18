#pragma once

#include "World/Actor/ServerActor.h"

#include <optional>
#include <string>
#include <vector>

class ServerNetworkHandler;

// the actor behind the minecraft:npc component: its skins come from npc_data.skin_list of the definition
class NpcActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:npc";

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

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    // the plain text a raw text json resolves to; anything else stays as it is
    static std::string resolveName(const std::string &rawName);

private:
    void _appendSkin(EntityDataMap &metadata) const;

    std::vector<Skin> mSkins;
    int32_t mSkinIndex = 0;
    std::string mRawName;
    std::string mSceneName;
};
