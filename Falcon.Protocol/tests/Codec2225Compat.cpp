#include "Protocol/Codec/ProtocolCodecRegistry.h"
#include "Protocol/Packet.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Packets/AddActorPacket.h"
#include "Protocol/Packets/ClientboundDataStorePacket.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"
#include "Protocol/Packets/SetPassengerOfBlockPacket.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/StructureBlockUpdatePacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/TransferPacket.h"

#include <iostream>
#include <memory>
#include <string>

// bytes recorded from the 2225 codec before 2250 was added: a 2225 client must keep getting exactly these
namespace {

    int gFailures = 0;

    std::string hex(const std::string &bytes) {
        static const char *digits = "0123456789abcdef";
        std::string out;
        for (unsigned char c: bytes) {
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        return out;
    }

    void check(const char *name, const Packet &packet, const PacketCodecContext &context, const char *expected) {
        BinaryStream stream;
        packet.writeWithHeader(stream, context);
        const std::string actual = hex(stream.getBuffer());
        if (actual == expected)
            return;

        std::cout << name << " changed for 2225: " << actual << std::endl;
        ++gFailures;
    }

}

int main() {
    BlockDefinitionRegistry blocks;
    ItemDefinitionRegistry items;
    std::shared_ptr<const ProtocolCodec> codec = ProtocolCodecRegistry::instance().find(2225);
    if (codec == nullptr) {
        std::cout << "no 2225 codec" << std::endl;
        return 1;
    }
    PacketCodecContext context(blocks, items, codec);

    CommandBlockUpdatePacket commandBlock;
    commandBlock.mBlock = true;
    commandBlock.mBlockPosition = Vector3i(1, -2, 3);
    commandBlock.mCommand = "say hi";
    commandBlock.mLastOutput = "out";
    commandBlock.mName = "n";
    commandBlock.mOutputTracked = true;
    commandBlock.mTickDelay = 7;
    commandBlock.mExecutingOnFirstTick = true;
    commandBlock.mLastOutputIsRawMessage = true;
    check("CommandBlockUpdate", commandBlock, context,
          "4e0102030600000006736179206869036f7574016e00010700000001");

    CommandOutputPacket output;
    output.mSuccessCount = 2;
    CommandOutputMessage message;
    message.mMessageId = "commands.say";
    message.mInternal = true;
    message.mParameters = {"a", "b"};
    message.mHasRawMessage = true;
    message.mRawMessage = "{}";
    output.mMessages = {message, message};
    output.mHasData = true;
    output.mData = "d";
    check("CommandOutput", output, context,
          "4f06706c617965720000000000000000000000000000000000ffffffffffffffff09616c6c6f75747075740200000002"
                "0c636f6d6d616e64732e7361790102016101620c636f6d6d616e64732e736179010201610162010164");

    StructureBlockUpdatePacket structure;
    structure.mBlockPosition = Vector3i(4, 5, 6);
    structure.mPowered = true;
    structure.mWaterlogged = true;
    structure.mKeepStructureVoid = true;
    check("StructureBlockUpdate", structure, context,
          "5a080a0c0000000001000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "010101");

    PlaySoundPacket sound;
    sound.mSound = "random.orb";
    sound.mPosition = Vector3f(1.0f, 2.0f, 3.0f);
    sound.mVolume = 1.0f;
    sound.mPitch = 0.5f;
    sound.mLoopCount = 3;
    sound.mBypassListenerRangeCheck = true;
    sound.mHasServerSoundHandle = true;
    sound.mServerSoundHandle = 42;
    sound.mHasPlaybackPosition = true;
    sound.mPlaybackPositionSeconds = 1.5f;
    sound.mHasStartDelay = true;
    sound.mStartDelaySeconds = 2.0f;
    check("PlaySound", sound, context,
          "560a72616e646f6d2e6f72621020300000803f0000003f0601012a00000000000000010000c03f");

    TextPacket text;
    text.mType = TextPacket::Type::JukeboxPopup;
    text.mMessage = "record.nowPlaying";
    text.mParameters = {"x"};
    text.mXuid = "1";
    text.mQueue = true;
    check("Text", text, context,
          "09000204117265636f72642e6e6f77506c6179696e6701017801310000");

    TransferPacket transfer;
    transfer.mAddress = "example.org";
    transfer.mPort = 19133;
    transfer.mReloadWorld = true;
    transfer.mHasScreenSettings = true;
    transfer.mScreenSettings.mHasTitleText = true;
    check("Transfer", transfer, context,
          "550b6578616d706c652e6f7267bd4a0100");

    ResourcePackStackPacket stack;
    stack.mGameVersion = "*";
    check("ResourcePackStack", stack, context,
          "070000012a000000000000");

    SetPassengerOfBlockPacket passenger;
    passenger.mPassengerUniqueId = -5;
    passenger.mHasData = true;
    passenger.mData.mBlockPosition = Vector3i(7, 8, 9);
    passenger.mData.mRotation = 90.0f;
    passenger.mData.mEmoteType = PassengerOfBlockEmoteType::Laying;
    passenger.mData.mBlockType = 99;
    check("SetPassengerOfBlock", passenger, context,
          "e50209010e10120000000000000000000000000000b4420000000002");

    AddActorPacket actor;
    actor.mHasPassengerOfBlock = true;
    actor.mPassengerOfBlock = passenger.mData;
    check("AddActor", actor, context,
          "0d0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "00010e10120000000000000000000000000000b4420000000002");

    ClientboundDataStorePacket dataStore;
    DataStoreAction update;
    update.mType = DataStoreActionType::Update;
    update.mUpdate.mDataStoreName = "store";
    update.mUpdate.mProperty = "prop";
    update.mUpdate.mPath = "a.b";
    update.mUpdate.mData.mType = DataStoreScalarType::String;
    update.mUpdate.mData.mStringValue = "v";
    update.mUpdate.mUpdateCount = 3;
    update.mUpdate.mPathUpdateCount = 4;
    update.mUpdate.mVariableName = "ignored";
    DataStoreAction change;
    change.mType = DataStoreActionType::Change;
    change.mChange.mDataStoreName = "store";
    change.mChange.mProperty = "prop";
    change.mChange.mUpdateCount = 5;
    change.mChange.mNewValue.mType = DataStoreChangeValueType::Bool;
    change.mChange.mNewValue.mBoolValue = true;
    DataStoreAction removal;
    removal.mType = DataStoreActionType::Removal;
    removal.mRemoval.mDataStoreName = "store";
    dataStore.mUpdates = {update, change, removal};
    check("ClientboundDataStore", dataStore, context,
          "ca0203000573746f72650470726f7003612e620201760300000004000000010573746f72650470726f70050000000100"
                "000001020573746f7265");

    ServerboundDataStorePacket serverStore;
    serverStore.mUpdate = update.mUpdate;
    check("ServerboundDataStore", serverStore, context,
          "cc020573746f72650470726f7003612e620201760300000004000000");

    StartGamePacket startGame;
    startGame.mHasServerConfigurationJoinInfo = true;
    startGame.mServerConfigurationJoinInfo.mHasPresenceConfiguration = true;
    startGame.mServerConfigurationJoinInfo.mPresenceConfiguration.mHasRichPresenceId = true;
    startGame.mServerConfigurationJoinInfo.mPresenceConfiguration.mRichPresenceId = "rp";
    startGame.mServerConfigurationJoinInfo.mHasManagedChat = true;
    check("StartGame", startGame, context,
          "0b0000000000000000000000000000000000000000000000000000000000000000000000020000020080010001000000"
                "000000000000000000000000000001010808010000000000000000000104000000000000000000000000000010000000"
                "100000000000000000000000000000000000000000000000000000000001000a00000000000000000000000000000000"
                "00000000000000000000000100010000010102727000000000");

    std::cout << (gFailures == 0 ? "2225 encodes as before" : "2225 encoding changed") << std::endl;
    return gFailures == 0 ? 0 : 1;
}
