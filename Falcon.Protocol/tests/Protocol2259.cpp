#include "Protocol/Codec/ProtocolCodecRegistry.h"
#include "Protocol/DataStoreCodec.h"
#include "Protocol/Packet.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Packets/AddActorPacket.h"
#include "Protocol/Packets/ChatPacket.h"
#include "Protocol/Packets/ClientboundAttributeLayerSyncPacket.h"
#include "Protocol/Packets/ClientboundDataStorePacket.h"
#include "Protocol/Packets/ClientboundMusicPlaybackFailurePacket.h"
#include "Protocol/Packets/ClientboundOnlineMusicPlaybackResponsePacket.h"
#include "Protocol/Packets/ClientboundPlayAudioContentPacket.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Packets/PlayerToggleArmorVisibilityRequestPacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"
#include "Protocol/Packets/ServerboundRequestOnlineMusicPlaybackPacket.h"
#include "Protocol/Packets/SetPassengerOfBlockPacket.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/StructureBlockUpdatePacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/TransferPacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/VirtualContainerClosePacket.h"
#include "Protocol/Packets/VirtualContainerOpenPacket.h"
#include "Protocol/ProtocolInfo.h"
#include "Protocol/Types/AudioContentRegistrationEntry.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

    BlockDefinitionRegistry gBlocks;
    ItemDefinitionRegistry gItems;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    PacketCodecContext contextFor(int version) {
        std::shared_ptr<const ProtocolCodec> codec = ProtocolCodecRegistry::instance().find(version);
        require(codec != nullptr, "no codec for " + std::to_string(version));
        return PacketCodecContext(gBlocks, gItems, codec);
    }

    std::string encode(const Packet &packet, const PacketCodecContext &context) {
        BinaryStream stream;
        packet.writeWithHeader(stream, context);
        return stream.getBuffer();
    }

    std::string body(const Packet &packet, int version) {
        const PacketCodecContext context = contextFor(version);
        BinaryStream stream;
        packet.write(stream, context);
        return stream.getBuffer();
    }

    // encodes, decodes into a fresh packet, checks nothing is left and the decoded packet encodes the same
    template<class PacketType>
    PacketType roundTrip(const PacketType &packet, int version) {
        const PacketCodecContext context = contextFor(version);
        const std::string bytes = encode(packet, context);

        ReadOnlyBinaryStream input(bytes);
        PacketType decoded;
        decoded.readHeader(input);
        context.getCodec().read(decoded, input, context);
        require(input.getRemainingLength() == 0,
                std::string(packet.getName()) + " left bytes unread for " + std::to_string(version));
        require(encode(decoded, context) == bytes,
                std::string(packet.getName()) + " encodes differently after decoding for " + std::to_string(version));
        return decoded;
    }

    PassengerOfBlockData passengerData() {
        PassengerOfBlockData data;
        data.mBlockPosition = Vector3i(7, 8, 9);
        data.mOffset = Vector3f(0.5f, 0.25f, -0.5f);
        data.mRotation = 90.0f;
        data.mRotationLimit = 45.0f;
        data.mEmoteType = PassengerOfBlockEmoteType::Laying;
        data.mBlockType = 0x1122334455667788ULL;
        return data;
    }

    void testRegistry() {
        ProtocolCodecRegistry &registry = ProtocolCodecRegistry::instance();
        require(ProtocolInfo::CURRENT_PROTOCOL == 2259, "current protocol is not 2259");
        require(registry.getDefault().getProtocolVersion() == 2259, "default codec is not 2259");
        require(registry.getProtocolVersions().back() == 2259, "2259 is not the newest codec");
        require(registry.find(2225) != nullptr, "2225 is no longer supported");

        const MinecraftPacketIds added[] = {
                MinecraftPacketIds::Chat, MinecraftPacketIds::ServerboundRequestOnlineMusicPlayback,
                MinecraftPacketIds::ClientboundOnlineMusicPlaybackResponse,
                MinecraftPacketIds::PlayerToggleArmorVisibilityRequest,
                MinecraftPacketIds::ClientboundMusicPlaybackFailure, MinecraftPacketIds::VirtualContainerOpen,
                MinecraftPacketIds::VirtualContainerClose,
        };
        const MinecraftPacketIds removed[] = {MinecraftPacketIds::AddVolumeEntity, MinecraftPacketIds::RemoveVolumeEntity};
        for (int version: {2225, 2193, 2169, 2168}) {
            const ProtocolCodec &codec = *registry.find(version);
            for (MinecraftPacketIds id: added)
                require(!codec.supports(id), "an older codec has a 2259 packet");
            for (MinecraftPacketIds id: removed)
                require(codec.supports(id), "an older codec lost the volume entity packets");
        }
        for (MinecraftPacketIds id: added)
            require(registry.getDefault().supports(id), "2259 lacks a new packet");
        for (MinecraftPacketIds id: removed)
            require(!registry.getDefault().supports(id), "2259 still has the volume entity packets");

        require((int) MinecraftPacketIds::Chat == 361, "chat id");
        require((int) MinecraftPacketIds::ServerboundRequestOnlineMusicPlayback == 362, "online music request id");
        require((int) MinecraftPacketIds::ClientboundOnlineMusicPlaybackResponse == 363, "online music response id");
        require((int) MinecraftPacketIds::PlayerToggleArmorVisibilityRequest == 364, "armor visibility id");
        require((int) MinecraftPacketIds::ClientboundMusicPlaybackFailure == 365, "music failure id");
        require((int) MinecraftPacketIds::VirtualContainerOpen == 366, "virtual container open id");
        require((int) MinecraftPacketIds::VirtualContainerClose == 367, "virtual container close id");
    }

    void testNewPackets() {
        ChatPacket chat;
        chat.mSignedChat.mKeyId = "key";
        chat.mSignedChat.mSenderId = "sender";
        chat.mSignedChat.mMessageId = "message";
        chat.mSignedChat.mScanText = "hello";
        chat.mSignedChat.mRedactedText = "";
        chat.mSignedChat.mAuthorName = "Steve";
        chat.mSignedChat.mSignature = "sig";
        chat.mHasRoomId = true;
        chat.mRoomId = "room";
        chat.mHasFormatting = true;
        chat.mFormatting.mNamePrefix = {true, "["};
        chat.mFormatting.mMessagePostfix = {true, "!"};
        const ChatPacket decodedChat = roundTrip(chat, 2259);
        require(decodedChat.mSignedChat.mSignature == "sig" && decodedChat.mFormatting.mNamePrefix.mValue == "["
                && !decodedChat.mFormatting.mNamePostfix.mPresent && decodedChat.mFormatting.mMessagePostfix.mPresent
                && decodedChat.mRoomId == "room" && decodedChat.mSignedChat.mAuthorName == "Steve",
                "chat fields lost");

        BinaryStream chatWire;
        for (const char *field: {"key", "sender", "message", "hello", "", "Steve", "sig"})
            chatWire.putString(field);
        chatWire.putBool(true);
        chatWire.putString("room");
        chatWire.putBool(true);
        chatWire.putBool(true);
        chatWire.putString("[");
        chatWire.putBool(false);
        chatWire.putBool(false);
        chatWire.putBool(true);
        chatWire.putString("!");
        require(body(chat, 2259) == chatWire.getBuffer(), "chat wire layout");

        ChatPacket unformatted = chat;
        unformatted.mHasFormatting = false;
        unformatted.mHasRoomId = false;
        const ChatPacket decodedUnformatted = roundTrip(unformatted, 2259);
        require(!decodedUnformatted.mHasFormatting && !decodedUnformatted.mHasRoomId, "chat without room or formatting");

        PlayerToggleArmorVisibilityRequestPacket armor;
        armor.mHideArmor = true;
        require(roundTrip(armor, 2259).mHideArmor, "armor visibility lost");
        require(body(armor, 2259) == std::string(1, '\x01'), "armor visibility wire layout");

        VirtualContainerOpenPacket open;
        open.mContainerId = 12;
        open.mSessionId = 300;
        open.mTitle = "container.title";
        open.mSlots.resize(27);
        const VirtualContainerOpenPacket decodedOpen = roundTrip(open, 2259);
        require(decodedOpen.mContainerId == 12 && decodedOpen.mSessionId == 300 && decodedOpen.mTitle == "container.title"
                && decodedOpen.mSlots.size() == 27, "virtual container open fields lost");
        BinaryStream openHeader;
        openHeader.putByte(12);
        openHeader.putUnsignedVarLong(300);
        openHeader.putString("container.title");
        openHeader.putArrayLength(27);
        require(body(open, 2259).compare(0, openHeader.getBuffer().size(), openHeader.getBuffer()) == 0,
                "virtual container open wire layout");

        VirtualContainerClosePacket close;
        close.mSessionId = 300;
        close.mKind = VirtualContainerCloseKind::Closed;
        close.mCause = VirtualContainerCloseCause::UiCancelled;
        const VirtualContainerClosePacket decodedClose = roundTrip(close, 2259);
        require(decodedClose.mSessionId == 300 && decodedClose.mKind == VirtualContainerCloseKind::Closed
                && decodedClose.mCause == VirtualContainerCloseCause::UiCancelled, "virtual container close fields lost");
        BinaryStream closeWire;
        closeWire.putUnsignedVarLong(300);
        closeWire.putByte(1);
        closeWire.putByte(7);
        require(body(close, 2259) == closeWire.getBuffer(), "virtual container close wire layout");

        ServerboundRequestOnlineMusicPlaybackPacket request;
        request.mRequestId = 0x01020304;
        request.mSoundEventName = "record.flurry";
        const ServerboundRequestOnlineMusicPlaybackPacket decodedRequest = roundTrip(request, 2259);
        require(decodedRequest.mRequestId == 0x01020304 && decodedRequest.mSoundEventName == "record.flurry",
                "online music request lost");
        BinaryStream requestWire;
        requestWire.putLInt(0x01020304);
        requestWire.putString("record.flurry");
        require(body(request, 2259) == requestWire.getBuffer(), "online music request wire layout");

        ClientboundOnlineMusicPlaybackResponsePacket response;
        response.mRequestId = 9;
        response.mState = MusicPlaybackState::PENDING;
        response.mSharedMetadata = "meta.jwt";
        response.mPlaybackContent = "content.jwt";
        const ClientboundOnlineMusicPlaybackResponsePacket decodedResponse = roundTrip(response, 2259);
        require(decodedResponse.mRequestId == 9 && decodedResponse.mState == "Pending"
                && decodedResponse.mPlaybackContent == "content.jwt", "online music response lost");

        ClientboundMusicPlaybackFailurePacket failure;
        failure.mFailures.push_back({MusicPlaybackState::NOT_FOUND, "record.unknown"});
        failure.mFailures.push_back({MusicPlaybackState::LOCAL, ""});
        const ClientboundMusicPlaybackFailurePacket decodedFailure = roundTrip(failure, 2259);
        require(decodedFailure.mFailures.size() == 2 && decodedFailure.mFailures[0].mState == "NotFound"
                && decodedFailure.mFailures[0].mDescription == "record.unknown", "music failures lost");
    }

    void testCommandPackets() {
        CommandBlockUpdatePacket commandBlock;
        commandBlock.mBlock = true;
        commandBlock.mBlockPosition = Vector3i(1, -2, 3);
        commandBlock.mCommand = "say hi";
        commandBlock.mExecutingOnFirstTick = true;
        commandBlock.mLastOutputIsRawMessage = true;
        require(roundTrip(commandBlock, 2259).mLastOutputIsRawMessage, "command block raw message flag lost");
        require(!roundTrip(commandBlock, 2225).mLastOutputIsRawMessage, "2225 command block read a raw message flag");
        const std::string current = body(commandBlock, 2259);
        const std::string older = body(commandBlock, 2225);
        require(current == older + std::string(1, '\x01'), "command block raw message flag is not the last byte");

        CommandOutputPacket output;
        CommandOutputMessage message;
        message.mMessageId = "commands.say";
        message.mParameters = {"a"};
        message.mHasRawMessage = true;
        message.mRawMessage = R"({"rawtext":[{"text":"hi"}]})";
        output.mMessages = {message, message};
        const CommandOutputPacket decodedOutput = roundTrip(output, 2259);
        require(decodedOutput.mMessages.size() == 2 && decodedOutput.mMessages[1].mHasRawMessage
                && decodedOutput.mMessages[1].mRawMessage == message.mRawMessage, "command output raw message lost");
        require(!roundTrip(output, 2225).mMessages[0].mHasRawMessage, "2225 command output read a raw message");
    }

    void testChangedPackets() {
        PlaySoundPacket sound;
        sound.mSound = "random.orb";
        sound.mHasPlaybackPosition = true;
        sound.mPlaybackPositionSeconds = 1.5f;
        sound.mHasStartDelay = true;
        sound.mStartDelaySeconds = 0.25f;
        const PlaySoundPacket decodedSound = roundTrip(sound, 2259);
        require(decodedSound.mHasStartDelay && decodedSound.mStartDelaySeconds == 0.25f, "start delay lost");
        require(!roundTrip(sound, 2225).mHasStartDelay, "2225 play sound read a start delay");
        BinaryStream delay;
        delay.putBool(true);
        delay.putLFloat(0.25f);
        BinaryStream noAttach;
        noAttach.putBool(false);
        require(body(sound, 2259) == body(sound, 2225) + delay.getBuffer() + noAttach.getBuffer(),
                "start delay and attach options layout");

        sound.mHasAttachedActor = true;
        sound.mAttachedActorRuntimeId = 1234;
        const PlaySoundPacket attached = roundTrip(sound, 2259);
        require(attached.mHasAttachedActor && attached.mAttachedActorRuntimeId == 1234, "attach options lost");
        require(!roundTrip(sound, 2225).mHasAttachedActor, "2225 play sound read attach options");
        BinaryStream attach;
        attach.putBool(true);
        attach.putUnsignedVarLong(1234);
        require(body(sound, 2259) == body(sound, 2225) + delay.getBuffer() + attach.getBuffer(), "attach options layout");

        ClientboundPlayAudioContentPacket audio;
        audio.mPlaybackType = AudioContentPlaybackType::CONCURRENT_MUSIC;
        audio.mSound = sound;
        require(roundTrip(audio, 2259).mSound.mHasStartDelay, "audio content start delay lost");

        TextPacket text;
        text.mType = TextPacket::Type::JukeboxPopup;
        text.mMessage = "record.nowPlaying";
        text.mParameters = {"Flurry"};
        text.mQueue = true;
        require(roundTrip(text, 2259).mQueue, "text queue flag lost");
        require(!roundTrip(text, 2225).mQueue, "2225 text read a queue flag");
        const std::string queued = body(text, 2259);
        const std::string older = body(text, 2225);
        require(queued.size() == older.size() + 1, "text queue flag is not one byte");

        TextPacket chat;
        chat.mType = TextPacket::Type::Chat;
        chat.mSourceName = "Steve";
        chat.mMessage = "hi";
        chat.mQueue = true;
        require(body(chat, 2259) == body(chat, 2225), "text queue flag written outside message and params");

        StructureBlockUpdatePacket structure;
        structure.mWaterlogged = true;
        structure.mKeepStructureVoid = true;
        require(roundTrip(structure, 2259).mKeepStructureVoid, "keep structure void lost");
        require(body(structure, 2259) == body(structure, 2225) + std::string(1, '\x01'),
                "keep structure void is not the last byte");

        TransferPacket transfer;
        transfer.mAddress = "example.org";
        transfer.mPort = 19133;
        transfer.mHasScreenSettings = true;
        transfer.mScreenSettings.mHasTitleText = true;
        transfer.mScreenSettings.mTitleText = "Moving";
        transfer.mScreenSettings.mHasCancelButtonDelay = true;
        transfer.mScreenSettings.mCancelButtonDelaySeconds = 5.0f;
        const TransferPacket decodedTransfer = roundTrip(transfer, 2259);
        require(decodedTransfer.mHasScreenSettings && decodedTransfer.mScreenSettings.mTitleText == "Moving"
                && !decodedTransfer.mScreenSettings.mHasBodyText
                && decodedTransfer.mScreenSettings.mCancelButtonDelaySeconds == 5.0f, "transfer screen settings lost");
        require(!roundTrip(transfer, 2225).mHasScreenSettings, "2225 transfer read screen settings");
        BinaryStream settings;
        settings.putBool(true);
        settings.putBool(true);
        settings.putString("Moving");
        for (int i = 0; i < 5; i++)
            settings.putBool(false);
        settings.putBool(true);
        settings.putLFloat(5.0f);
        require(body(transfer, 2259) == body(transfer, 2225) + settings.getBuffer(), "transfer screen settings layout");

        ResourcePackStackPacket stack;
        stack.mGameVersion = "*";
        stack.mEditorPackTypes = EditorPackType::CORE | EditorPackType::ANNOTATION;
        require(roundTrip(stack, 2259).mEditorPackTypes == 3, "editor pack types lost");
        const std::string stackCurrent = body(stack, 2259);
        const std::string stackOlder = body(stack, 2225);
        require(stackCurrent.back() == '\x03' && stackOlder.back() == '\x01'
                && stackCurrent.size() == stackOlder.size(), "editor pack types byte");

        SetPassengerOfBlockPacket passenger;
        passenger.mPassengerUniqueId = -5;
        passenger.mHasData = true;
        passenger.mData = passengerData();
        require(roundTrip(passenger, 2259).mData.mBlockType == 0x1122334455667788ULL, "passenger block type lost");
        require(roundTrip(passenger, 2225).mData.mBlockType == 0, "2225 passenger read a block type");
        passenger.mData.mPackId = Uuid(0x0102030405060708ULL, 0x090A0B0C0D0E0F10ULL);
        const SetPassengerOfBlockPacket decodedPassenger = roundTrip(passenger, 2259);
        require(decodedPassenger.mData.mPackId.mostSignificantBits == 0x0102030405060708ULL
                && decodedPassenger.mData.mPackId.leastSignificantBits == 0x090A0B0C0D0E0F10ULL, "passenger pack lost");
        BinaryStream blockType;
        blockType.putLLong(0x1122334455667788ULL);
        blockType.putUuid(passenger.mData.mPackId);
        require(body(passenger, 2259) == body(passenger, 2225) + blockType.getBuffer(), "passenger block type layout");

        AddActorPacket actor;
        actor.mHasPassengerOfBlock = true;
        actor.mPassengerOfBlock = passengerData();
        require(roundTrip(actor, 2259).mPassengerOfBlock.mBlockType == 0x1122334455667788ULL,
                "add actor passenger block type lost");
        require(body(actor, 2259).size() == body(actor, 2225).size() + 24, "add actor passenger block type size");

        StartGamePacket startGame;
        startGame.mHasServerConfigurationJoinInfo = true;
        startGame.mServerConfigurationJoinInfo.mHasManagedChat = true;
        startGame.mServerConfigurationJoinInfo.mManagedChat.mEnabled = true;
        const StartGamePacket decodedStart = roundTrip(startGame, 2259);
        require(decodedStart.mServerConfigurationJoinInfo.mHasManagedChat
                && decodedStart.mServerConfigurationJoinInfo.mManagedChat.mEnabled, "managed chat lost");
        require(!roundTrip(startGame, 2225).mServerConfigurationJoinInfo.mHasManagedChat,
                "2225 start game read managed chat");
        require(body(startGame, 2259).size() == body(startGame, 2225).size() + 2, "managed chat size");

        NetworkSettingsPacket network;
        network.mSctpRecoveryProfile = NetworkSettingsPacket::SctpRecoveryProfile::Balanced;
        require(roundTrip(network, 2259).mSctpRecoveryProfile == NetworkSettingsPacket::SctpRecoveryProfile::Balanced,
                "sctp recovery profile lost");
        require(body(network, 2259) == body(network, 2225) + std::string(1, '\x02'), "sctp recovery profile layout");
    }

    void testDataStore() {
        DataStoreAction update;
        update.mType = DataStoreActionType::Update;
        update.mUpdate.mVariableName = "screen.title";
        update.mUpdate.mData.mType = DataStoreScalarType::Number;
        update.mUpdate.mData.mNumberValue = 2.5;
        update.mUpdate.mUpdateCount = 7;
        DataStoreAction change;
        change.mType = DataStoreActionType::Change;
        change.mChange.mVariableName = "screen.visible";
        change.mChange.mUpdateCount = 3;
        change.mChange.mNewVariableValue.mType = DataStoreScalarType::Bool;
        change.mChange.mNewVariableValue.mBoolValue = true;
        DataStoreAction removal;
        removal.mType = DataStoreActionType::Removal;
        removal.mRemoval.mVariableName = "screen.old";

        ClientboundDataStorePacket packet;
        packet.mUpdates = {update, change, removal};
        const ClientboundDataStorePacket decoded = roundTrip(packet, 2259);
        require(decoded.mUpdates.size() == 3 && decoded.mUpdates[0].mUpdate.mVariableName == "screen.title"
                && decoded.mUpdates[1].mChange.mNewVariableValue.mBoolValue
                && decoded.mUpdates[2].mRemoval.mVariableName == "screen.old", "data store variables lost");

        BinaryStream wire;
        wire.putUnsignedVarInt(3);
        wire.putUnsignedVarInt(0);
        wire.putString("screen.title");
        wire.putUnsignedVarInt(0);
        wire.putLDouble(2.5);
        wire.putLInt(7);
        wire.putUnsignedVarInt(1);
        wire.putString("screen.visible");
        wire.putLInt(3);
        wire.putUnsignedVarInt(1);
        wire.putBool(true);
        wire.putUnsignedVarInt(2);
        wire.putString("screen.old");
        require(body(packet, 2259) == wire.getBuffer(), "data store variables wire layout");

        ServerboundDataStorePacket serverbound;
        serverbound.mUpdate = update.mUpdate;
        require(roundTrip(serverbound, 2259).mUpdate.mVariableName == "screen.title", "serverbound variable lost");

        ServerboundDataStorePacket older;
        older.mUpdate.mDataStoreName = "store";
        older.mUpdate.mProperty = "prop";
        older.mUpdate.mPath = "a";
        older.mUpdate.mPathUpdateCount = 4;
        const ServerboundDataStorePacket decodedOlder = roundTrip(older, 2225);
        require(decodedOlder.mUpdate.mPath == "a" && decodedOlder.mUpdate.mPathUpdateCount == 4,
                "2225 data store path lost");
    }

    void testAttributeLayerString() {
        ClientboundAttributeLayerSyncPacket packet;
        AttributeLayerData layer;
        layer.mLayerName = "layer";
        EnvironmentAttributeData attribute;
        attribute.mAttributeName = "minecraft:sky_texture";
        attribute.mAttribute.mType = EnvironmentAttributeValueType::String;
        attribute.mAttribute.mStringValue = "textures/sky";
        layer.mAttributes.push_back(attribute);
        packet.mData.mLayers.push_back(layer);
        const ClientboundAttributeLayerSyncPacket decoded = roundTrip(packet, 2259);
        const EnvironmentAttributeValue &value = decoded.mData.mLayers[0].mAttributes[0].mAttribute;
        require(value.mType == EnvironmentAttributeValueType::String && value.mStringValue == "textures/sky",
                "string attribute lost");
    }

}

int main() {
    try {
        testRegistry();
        testNewPackets();
        testCommandPackets();
        testChangedPackets();
        testDataStore();
        testAttributeLayerString();
    } catch (const std::exception &error) {
        std::cout << "FAIL: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "protocol 2259 tests passed" << std::endl;
    return 0;
}
