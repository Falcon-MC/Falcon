#pragma once

#include <cstdint>
#include <string>
#include <vector>

// a button or an automatic step of an npc dialogue, as the game stores it in its actions json
struct NpcAction {
    enum class Type : int32_t {
        Url = 0,
        Command = 1
    };

    // when the commands run: a button press, or the dialogue closing or opening
    enum class Mode : int32_t {
        Button = 0,
        OnClose = 1,
        OnOpen = 2
    };

    struct Command {
        std::string mLine;
        int32_t mVersion = 0;
    };

    // the command version the game stamps on the lines it splits out of an edited action
    static constexpr int32_t COMMAND_VERSION = 55;

    Type mType = Type::Command;
    Mode mMode = Mode::Button;
    std::string mText;
    std::string mButtonName;
    std::vector<Command> mCommands;

    // the lines of mText, trimmed, empty ones dropped: the commands the game keeps for an edited action
    void splitCommands();

    bool operator==(const NpcAction &other) const;

    // entries without a numeric type and mode or a string text and button name are dropped
    static std::vector<NpcAction> parseList(const std::string &json);

    // empty for no actions, as the game leaves the actor data
    static std::string writeList(const std::vector<NpcAction> &actions);
};
