#pragma once

#include "Core/NBT/Tag.h"
#include "Core/Utility/BinaryStream.h"
#include "Protocol/Types/GatheringsConfigurationJoinInfo.h"

#include <string>
#include <vector>

enum class GameType : int {
    Survival = 0,
    Creative = 1,
    Adventure = 2,
    SurvivalViewer = 3,
    CreativeViewer = 4,
    Default = 5,
    Spectator = 6
};

enum class SpawnBiomeType : int {
    Default = 0,
    UserDefined = 1
};

enum class EditorWorldType : int {
    NonEditor = 0,
    EditorProject = 1,
    EditorTestLevel = 2,
    EditorRealmsUpload = 3
};

enum class EditorLevelMigrationVersion : int8_t {
    Legacy = 0,
    EditorAchievements = 1
};

enum class GamePublishSetting : int {
    NoMultiPlay = 0,
    InviteOnly = 1,
    FriendsOnly = 2,
    FriendsOfFriends = 3,
    Public = 4
};

enum class PlayerPermission : int {
    Visitor = 0,
    Member = 1,
    Operator = 2,
    Custom = 3
};

enum class ChatRestrictionLevel : int {
    None = 0,
    Dropped = 1,
    Disabled = 2
};

struct EduSharedUriResource {
    std::string mButtonName;
    std::string mLinkUri;
};

struct NetworkPermissions {
    bool mServerAuthSounds = false;
};

struct ExperimentData {
    std::string mName;
    bool mEnabled = false;
};

struct BlockPropertyData {
    std::string mName;
    Tag mProperties = Tag::ofCompound();
};

struct ClientStoreEntrypointConfiguration {
    std::string mStoreId;
    std::string mStoreName;
};

struct PresenceConfiguration {
    bool mHasRichPresenceId = false;
    std::string mRichPresenceId;
};

struct ServerConfigurationJoinInfo {
    bool mHasGatheringsConfiguration = false;
    GatheringsConfigurationJoinInfo mGatheringsConfiguration;
    bool mHasClientStoreEntrypoint = false;
    ClientStoreEntrypointConfiguration mClientStoreEntrypoint;
    bool mHasPresenceConfiguration = false;
    PresenceConfiguration mPresenceConfiguration;

    void write(BinaryStream &stream) const;

    void read(ReadOnlyBinaryStream &stream);
};

struct GameRuleData {
    enum class Type : int {
        Unknown = 0,
        Bool = 1,
        Int = 2,
        Float = 3
    };

    std::string mName;
    bool mEditable = false;
    Type mType = Type::Bool;

    bool mBoolValue = false;
    int32_t mIntValue = 0;
    float mFloatValue = 0.0f;

    static GameRuleData ofBool(const std::string &name, bool value, bool editable = true);

    static GameRuleData ofInt(const std::string &name, int32_t value, bool editable = true);

    static GameRuleData ofFloat(const std::string &name, float value, bool editable = true);

    void write(BinaryStream &stream) const;

    void read(ReadOnlyBinaryStream &stream);
};

void writeExperiments(BinaryStream &stream, const std::vector<ExperimentData> &experiments);

void readExperiments(ReadOnlyBinaryStream &stream, std::vector<ExperimentData> &experiments);
