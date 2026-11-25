#include "Protocol/Packets/AvailableCommandsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <algorithm>
#include <array>

namespace {

    const std::array<std::string, 6> PERMISSION_NAMES = {
        "any", "gamedirectors", "admin", "host", "owner", "internal"
    };

    int32_t indexOf(const std::vector<std::string> &values, const std::string &value) {
        const auto it = std::find(values.begin(), values.end(), value);
        return it == values.end() ? -1 : (int32_t) std::distance(values.begin(), it);
    }

    template<typename T>
    int32_t indexOf(const std::vector<T> &values, const T &value) {
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i] == value)
                return (int32_t) i;
        }
        return -1;
    }

    void addUnique(std::vector<std::string> &values, const std::string &value) {
        if (indexOf(values, value) == -1)
            values.push_back(value);
    }

}

bool operator==(const CommandEnumData &lhs, const CommandEnumData &rhs) {
    return lhs.mName == rhs.mName && lhs.mValues == rhs.mValues && lhs.mIsSoft == rhs.mIsSoft;
}

bool operator==(const ChainedSubCommandData &lhs, const ChainedSubCommandData &rhs) {
    return lhs.mName == rhs.mName;
}

AvailableCommandsPacket::AvailableCommandsPacket() = default;

void AvailableCommandsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    std::vector<std::string> enumValues;
    std::vector<std::string> subCommandValues;
    std::vector<std::string> postFixes;
    std::vector<CommandEnumData> enums;
    std::vector<CommandEnumData> softEnums;
    std::vector<ChainedSubCommandData> subCommandData;

    for (const CommandData &command: mCommands) {
        if (command.mHasAliases) {
            for (const std::string &value: command.mAliases.mValues)
                addUnique(enumValues, value);

            if (indexOf(enums, command.mAliases) == -1)
                enums.push_back(command.mAliases);
        }

        for (const ChainedSubCommandData &subCommand: command.mSubCommands) {
            if (indexOf(subCommandData, subCommand) != -1)
                continue;

            subCommandData.push_back(subCommand);
            for (const ChainedSubCommandValue &value: subCommand.mValues) {
                addUnique(subCommandValues, value.mFirst);
                addUnique(subCommandValues, value.mSecond);
            }
        }

        for (const CommandOverloadData &overload: command.mOverloads) {
            for (const CommandParamData &parameter: overload.mParameters) {
                if (parameter.mHasEnumData) {
                    if (parameter.mEnumData.mIsSoft) {
                        if (indexOf(softEnums, parameter.mEnumData) == -1)
                            softEnums.push_back(parameter.mEnumData);
                    } else {
                        for (const std::string &value: parameter.mEnumData.mValues)
                            addUnique(enumValues, value);

                        if (indexOf(enums, parameter.mEnumData) == -1)
                            enums.push_back(parameter.mEnumData);
                    }
                }
            }
        }
    }

    stream.putArrayLength((uint32_t) enumValues.size());
    for (const std::string &value: enumValues)
        stream.putString(value);

    stream.putArrayLength((uint32_t) subCommandValues.size());
    for (const std::string &value: subCommandValues)
        stream.putString(value);

    stream.putArrayLength((uint32_t) postFixes.size());
    for (const std::string &value: postFixes)
        stream.putString(value);

    stream.putArrayLength((uint32_t) enums.size());
    for (const CommandEnumData &commandEnum: enums) {
        stream.putString(commandEnum.mName);
        stream.putUnsignedVarInt((uint32_t) commandEnum.mValues.size());
        for (const std::string &value: commandEnum.mValues)
            stream.putLInt((uint32_t) indexOf(enumValues, value));
    }

    stream.putArrayLength((uint32_t) subCommandData.size());
    for (const ChainedSubCommandData &subCommand: subCommandData) {
        stream.putString(subCommand.mName);
        stream.putArrayLength((uint32_t) subCommand.mValues.size());
        for (const ChainedSubCommandValue &value: subCommand.mValues) {
            stream.putUnsignedVarInt((uint32_t) indexOf(subCommandValues, value.mFirst));
            stream.putUnsignedVarInt((uint32_t) indexOf(subCommandValues, value.mSecond));
        }
    }

    stream.putArrayLength((uint32_t) mCommands.size());
    for (const CommandData &command: mCommands) {
        stream.putString(command.mName);
        stream.putString(command.mDescription);
        stream.putLShort(command.mFlags);
        stream.putString(PERMISSION_NAMES[(size_t) command.mPermission]);

        stream.putLInt(command.mHasAliases ? (uint32_t) indexOf(enums, command.mAliases) : (uint32_t) -1);

        stream.putArrayLength((uint32_t) command.mSubCommands.size());
        for (const ChainedSubCommandData &subCommand: command.mSubCommands)
            stream.putLInt((uint32_t) indexOf(subCommandData, subCommand));

        stream.putUnsignedVarInt((uint32_t) command.mOverloads.size());
        for (const CommandOverloadData &overload: command.mOverloads) {
            stream.putBool(overload.mChaining);
            stream.putUnsignedVarInt((uint32_t) overload.mParameters.size());

            for (const CommandParamData &parameter: overload.mParameters) {
                stream.putString(parameter.mName);

                uint32_t symbol;
                if (parameter.mHasEnumData) {
                    if (parameter.mEnumData.mIsSoft) {
                        symbol = (uint32_t) indexOf(softEnums, parameter.mEnumData) | ARG_FLAG_SOFT_ENUM | ARG_FLAG_VALID;
                    } else {
                        symbol = (uint32_t) indexOf(enums, parameter.mEnumData) | ARG_FLAG_ENUM | ARG_FLAG_VALID;
                    }
                } else {
                    symbol = (uint32_t) parameter.mType | ARG_FLAG_VALID;
                }

                stream.putLInt(symbol);
                stream.putBool(parameter.mOptional);
                stream.putByte(0);
            }
        }
    }

    stream.putArrayLength((uint32_t) softEnums.size());
    for (const CommandEnumData &softEnum: softEnums) {
        stream.putString(softEnum.mName);
        stream.putArrayLength((uint32_t) softEnum.mValues.size());
        for (const std::string &value: softEnum.mValues)
            stream.putString(value);
    }

    stream.putArrayLength(0);
}

void AvailableCommandsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    std::vector<std::string> enumValues;
    const uint32_t enumValueCount = stream.getArrayLength();
    enumValues.reserve(enumValueCount);
    for (uint32_t i = 0; i < enumValueCount; i++)
        enumValues.push_back(stream.getString());

    std::vector<std::string> subCommandValues;
    const uint32_t subCommandValueCount = stream.getArrayLength();
    subCommandValues.reserve(subCommandValueCount);
    for (uint32_t i = 0; i < subCommandValueCount; i++)
        subCommandValues.push_back(stream.getString());

    std::vector<std::string> postFixes;
    const uint32_t postFixCount = stream.getArrayLength();
    postFixes.reserve(postFixCount);
    for (uint32_t i = 0; i < postFixCount; i++)
        postFixes.push_back(stream.getString());

    std::vector<CommandEnumData> enums;
    const uint32_t enumCount = stream.getArrayLength();
    enums.reserve(enumCount);
    for (uint32_t i = 0; i < enumCount; i++) {
        CommandEnumData commandEnum;
        commandEnum.mName = stream.getString();

        const uint32_t valueCount = stream.getUnsignedVarInt();
        for (uint32_t j = 0; j < valueCount; j++) {
            const uint32_t valueIndex = stream.getLInt();
            if (valueIndex < enumValues.size())
                commandEnum.mValues.push_back(enumValues[valueIndex]);
        }

        enums.push_back(std::move(commandEnum));
    }

    std::vector<ChainedSubCommandData> subCommandData;
    const uint32_t subCommandCount = stream.getArrayLength();
    subCommandData.reserve(subCommandCount);
    for (uint32_t i = 0; i < subCommandCount; i++) {
        ChainedSubCommandData subCommand;
        subCommand.mName = stream.getString();

        const uint32_t valueCount = stream.getArrayLength();
        for (uint32_t j = 0; j < valueCount; j++) {
            ChainedSubCommandValue value;
            const uint32_t firstIndex = stream.getUnsignedVarInt();
            const uint32_t secondIndex = stream.getUnsignedVarInt();
            if (firstIndex < subCommandValues.size())
                value.mFirst = subCommandValues[firstIndex];
            if (secondIndex < subCommandValues.size())
                value.mSecond = subCommandValues[secondIndex];
            subCommand.mValues.push_back(value);
        }

        subCommandData.push_back(std::move(subCommand));
    }

    mCommands.clear();
    const uint32_t commandCount = stream.getArrayLength();
    mCommands.reserve(commandCount);

    for (uint32_t i = 0; i < commandCount; i++) {
        CommandData command;
        command.mName = stream.getString();
        command.mDescription = stream.getString();
        command.mFlags = stream.getLShort();

        const std::string permissionName = stream.getString();
        const auto permissionIt = std::find(PERMISSION_NAMES.begin(), PERMISSION_NAMES.end(), permissionName);
        command.mPermission = permissionIt == PERMISSION_NAMES.end()
            ? CommandPermission::Any
            : (CommandPermission) std::distance(PERMISSION_NAMES.begin(), permissionIt);

        const uint32_t aliasIndex = stream.getLInt();
        if (aliasIndex != (uint32_t) -1 && aliasIndex < enums.size()) {
            command.mHasAliases = true;
            command.mAliases = enums[aliasIndex];
        }

        const uint32_t commandSubCommandCount = stream.getArrayLength();
        for (uint32_t j = 0; j < commandSubCommandCount; j++) {
            const uint32_t subCommandIndex = stream.getLInt();
            if (subCommandIndex < subCommandData.size())
                command.mSubCommands.push_back(subCommandData[subCommandIndex]);
        }

        const uint32_t overloadCount = stream.getUnsignedVarInt();
        for (uint32_t j = 0; j < overloadCount; j++) {
            CommandOverloadData overload;
            overload.mChaining = stream.getBool();

            const uint32_t parameterCount = stream.getUnsignedVarInt();
            for (uint32_t k = 0; k < parameterCount; k++) {
                CommandParamData parameter;
                parameter.mName = stream.getString();

                const uint32_t symbol = stream.getLInt();
                if ((symbol & ARG_FLAG_POSTFIX) != 0) {
                    parameter.mHasType = true;
                    parameter.mType = CommandParamType::RawText;
                } else if ((symbol & ARG_FLAG_VALID) != 0) {
                    if ((symbol & ARG_FLAG_SOFT_ENUM) != 0) {
                        const uint32_t softIndex = symbol & ~(ARG_FLAG_SOFT_ENUM | ARG_FLAG_VALID);
                        parameter.mHasEnumData = true;
                        parameter.mEnumData.mIsSoft = true;
                        parameter.mEnumData.mName = "softEnum" + std::to_string(softIndex);
                    } else if ((symbol & ARG_FLAG_ENUM) != 0) {
                        const uint32_t enumIndex = symbol & ~(ARG_FLAG_ENUM | ARG_FLAG_VALID);
                        if (enumIndex < enums.size()) {
                            parameter.mHasEnumData = true;
                            parameter.mEnumData = enums[enumIndex];
                        }
                    } else {
                        parameter.mHasType = true;
                        parameter.mType = (CommandParamType) (symbol & ~ARG_FLAG_VALID);
                    }
                }

                parameter.mOptional = stream.getBool();
                stream.getByte();

                overload.mParameters.push_back(std::move(parameter));
            }

            command.mOverloads.push_back(std::move(overload));
        }

        mCommands.push_back(std::move(command));
    }

    const uint32_t softEnumCount = stream.getArrayLength();
    for (uint32_t i = 0; i < softEnumCount; i++) {
        stream.getString();
        const uint32_t valueCount = stream.getArrayLength();
        for (uint32_t j = 0; j < valueCount; j++)
            stream.getString();
    }

    const uint32_t constraintCount = stream.getArrayLength();
    for (uint32_t i = 0; i < constraintCount; i++) {
        stream.getLInt();
        stream.getLInt();
        const uint32_t optionCount = stream.getArrayLength();
        for (uint32_t j = 0; j < optionCount; j++)
            stream.getByte();
    }
}

void AvailableCommandsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
