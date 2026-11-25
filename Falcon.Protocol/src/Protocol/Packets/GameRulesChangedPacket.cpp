#include "Protocol/Packets/GameRulesChangedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeGameRule(BinaryStream &stream, const ChangedGameRuleData &rule) {
        stream.putString(rule.mName);
        stream.putBool(rule.mEditable);
        stream.putUnsignedVarInt((uint32_t) rule.mType);

        switch (rule.mType) {
            case ChangedGameRuleType::Bool:
                stream.putBool(rule.mBoolValue);
                break;
            case ChangedGameRuleType::Int:
                stream.putLInt((uint32_t) rule.mIntValue);
                break;
            case ChangedGameRuleType::Float:
                stream.putLFloat(rule.mFloatValue);
                break;
        }
    }

    ChangedGameRuleData readGameRule(ReadOnlyBinaryStream &stream) {
        ChangedGameRuleData rule;
        rule.mName = stream.getString();
        rule.mEditable = stream.getBool();
        rule.mType = (ChangedGameRuleType) stream.getUnsignedVarInt();

        switch (rule.mType) {
            case ChangedGameRuleType::Bool:
                rule.mBoolValue = stream.getBool();
                break;
            case ChangedGameRuleType::Int:
                rule.mIntValue = (int32_t) stream.getLInt();
                break;
            case ChangedGameRuleType::Float:
                rule.mFloatValue = stream.getLFloat();
                break;
        }

        return rule;
    }

}

GameRulesChangedPacket::GameRulesChangedPacket() = default;

void GameRulesChangedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mGameRules.size());
    for (const ChangedGameRuleData &rule : mGameRules) {
        writeGameRule(stream, rule);
    }
}

void GameRulesChangedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t count = stream.getArrayLength();
    mGameRules.clear();
    mGameRules.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mGameRules.push_back(readGameRule(stream));
    }
}

void GameRulesChangedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
