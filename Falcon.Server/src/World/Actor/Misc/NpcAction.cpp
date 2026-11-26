#include "World/Actor/Misc/NpcAction.h"

#include "Core/Json/Json.h"

#include <memory>

namespace {
    bool isBlank(char character) {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r';
    }
}

void NpcAction::splitCommands() {
    mCommands.clear();
    size_t start = 0;
    while (start <= mText.size()) {
        size_t end = mText.find('\n', start);
        if (end == std::string::npos)
            end = mText.size();
        size_t first = start, last = end;
        while (first < last && isBlank(mText[first]))
            ++first;
        while (last > first && isBlank(mText[last - 1]))
            --last;
        if (last > first)
            mCommands.push_back({mText.substr(first, last - first), COMMAND_VERSION});
        start = end + 1;
    }
}

bool NpcAction::operator==(const NpcAction &other) const {
    if (mType != other.mType || mText != other.mText || mButtonName != other.mButtonName)
        return false;
    if (mType == Type::Url)
        return true;
    if (mMode != other.mMode || mCommands.size() != other.mCommands.size())
        return false;
    for (size_t index = 0; index < mCommands.size(); ++index) {
        if (mCommands[index].mLine != other.mCommands[index].mLine
            || mCommands[index].mVersion != other.mCommands[index].mVersion)
            return false;
    }
    return true;
}

std::vector<NpcAction> NpcAction::parseList(const std::string &json) {
    std::vector<NpcAction> actions;
    const std::unique_ptr<json::Value> parsed = json::parse(json);
    if (parsed == nullptr || !parsed->isArray())
        return actions;
    for (const std::unique_ptr<json::Value> &entry: parsed->mArray) {
        if (!entry->isObject())
            continue;
        const json::Value *type = entry->get("type");
        const json::Value *mode = entry->get("mode");
        const json::Value *text = entry->get("text");
        const json::Value *buttonName = entry->get("button_name");
        if (type == nullptr || !type->isNumber() || mode == nullptr || !mode->isNumber() || text == nullptr
            || !text->isString() || buttonName == nullptr || !buttonName->isString())
            continue;
        const int32_t kind = type->integer();
        if (kind != (int32_t) Type::Url && kind != (int32_t) Type::Command)
            continue;
        NpcAction action;
        action.mType = (Type) kind;
        action.mText = text->mString;
        action.mButtonName = buttonName->mString;
        if (action.mType == Type::Command) {
            const int32_t when = mode->integer();
            action.mMode = when == (int32_t) Mode::OnClose || when == (int32_t) Mode::OnOpen ? (Mode) when
                                                                                              : Mode::Button;
            const json::Value *data = entry->get("data");
            if (data != nullptr && data->isArray()) {
                for (const std::unique_ptr<json::Value> &line: data->mArray) {
                    const json::Value *command = line->get("cmd_line");
                    const json::Value *version = line->get("cmd_ver");
                    if (command == nullptr || !command->isString() || version == nullptr || !version->isNumber())
                        continue;
                    action.mCommands.push_back({command->mString, version->integer()});
                }
            }
        }
        actions.push_back(std::move(action));
    }
    return actions;
}

std::string NpcAction::writeList(const std::vector<NpcAction> &actions) {
    if (actions.empty())
        return std::string();
    std::unique_ptr<json::Value> list = json::Value::ofArray();
    for (const NpcAction &action: actions) {
        std::unique_ptr<json::Value> entry = json::Value::ofObject();
        entry->set("type", json::Value::ofInteger((int64_t) action.mType));
        entry->set("mode", json::Value::ofInteger((int64_t) action.mMode));
        entry->set("text", json::Value::ofString(action.mText));
        entry->set("button_name", json::Value::ofString(action.mButtonName));
        if (action.mType == Type::Command) {
            std::unique_ptr<json::Value> data = json::Value::ofArray();
            for (const Command &command: action.mCommands) {
                std::unique_ptr<json::Value> line = json::Value::ofObject();
                line->set("cmd_line", json::Value::ofString(command.mLine));
                line->set("cmd_ver", json::Value::ofInteger(command.mVersion));
                data->push(std::move(line));
            }
            entry->set("data", std::move(data));
        }
        list->push(std::move(entry));
    }
    return json::stringify(*list);
}
