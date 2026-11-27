#include "Scripting/Content/PackBlockState.h"

#include "Core/Json/Json.h"
#include "World/Level/Block/BlockPaletteRegistry.h"

#include <cmath>

std::optional<BlockState> PackBlockState::read(const json::Value &value) {
    const auto *identifier = value.isString() ? &value : value.get("name");
    if (identifier == nullptr || !identifier->isString() || identifier->mString.empty())
        return std::nullopt;

    std::string name = identifier->mString;
    if (name.find(':') == std::string::npos)
        name.insert(0, "minecraft:");
    const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
    Tag states = defaults == nullptr ? Tag::ofCompound() : *defaults;
    if (const auto *overrides = value.get("states")) {
        if (!overrides->isObject())
            return std::nullopt;
        for (const auto &key: overrides->mKeys) {
            const auto &state = *overrides->mObject.at(key);
            if (state.isString())
                states.putString(key, state.mString);
            else if (state.mType == json::Value::Type::Boolean)
                states.putByte(key, state.mBoolean);
            else if (state.isNumber() && state.mNumber >= INT32_MIN && state.mNumber <= INT32_MAX
                     && state.mNumber == std::trunc(state.mNumber))
                states.putInt(key, (int32_t) state.mNumber);
            else
                return std::nullopt;
        }
    }
    return BlockState(name, std::move(states));
}
