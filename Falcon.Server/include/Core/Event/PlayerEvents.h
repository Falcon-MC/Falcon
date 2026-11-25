#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BlockState;
class ConnectionRequest;
class Level;
class NetworkIdentifier;
class ServerPlayer;
struct ResourcePackOffer;

// a parsed login before its chain is verified; the handler may only add the authentication requirement
struct PlayerLoginAttemptBeforeEvent : public CancellableEvent {
    const NetworkIdentifier &mSource;
    const ConnectionRequest &mRequest;
    int32_t mClientPort;
    bool mRequireAuthentication;
    std::string mKickReason = "disconnectionScreen.noReason";

    PlayerLoginAttemptBeforeEvent(const NetworkIdentifier &source, const ConnectionRequest &request, int32_t port,
                                  bool requireAuthentication)
            : mSource(source), mRequest(request), mClientPort(port), mRequireAuthentication(requireAuthentication) {}
};

// a login past the ban, allow list and player limit checks
struct PlayerPreLoginBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    int32_t mClientPort;
    const std::string &mClientData;
    std::string mKickReason = "disconnectionScreen.noReason";

    PlayerPreLoginBeforeEvent(ServerPlayer &player, int32_t port, const std::string &clientData)
            : mPlayer(player), mClientPort(port), mClientData(clientData) {}
};

// a login under the name or account of a player already online; cancelling keeps the existing session
struct PlayerDuplicateLoginBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    ServerPlayer &mExisting;
    bool mSameAccount;
    int32_t mClientPort;
    // shared by every duplicate of this login
    std::string &mKickReason;

    PlayerDuplicateLoginBeforeEvent(ServerPlayer &player, ServerPlayer &existing, bool sameAccount, int32_t port,
                                    std::string &kickReason)
            : mPlayer(player), mExisting(existing), mSameAccount(sameAccount), mClientPort(port),
              mKickReason(kickReason) {}
};

struct PlayerResourcePackOfferBeforeEvent : public Event {
    ServerPlayer &mPlayer;
    std::shared_ptr<const ResourcePackOffer> &mOffer;

    PlayerResourcePackOfferBeforeEvent(ServerPlayer &player, std::shared_ptr<const ResourcePackOffer> &offer)
            : mPlayer(player), mOffer(offer) {}
};

// the player with its saved data loaded, before the start game goes out
struct PlayerCreationAfterEvent : public Event {
    ServerPlayer &mPlayer;
    int32_t mClientPort;

    PlayerCreationAfterEvent(ServerPlayer &player, int32_t port) : mPlayer(player), mClientPort(port) {}
};

// a sprint, swim, glide or flight state about to change
struct PlayerToggleBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    bool mActive;

    PlayerToggleBeforeEvent(ServerPlayer &player, bool active) : mPlayer(player), mActive(active) {}
};

struct PlayerToggleSneakBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    bool mSneaking;
    // the input pressed the sneak key, rather than only carrying the sneaking state
    bool mPressed;

    PlayerToggleSneakBeforeEvent(ServerPlayer &player, bool sneaking, bool pressed)
            : mPlayer(player), mSneaking(sneaking), mPressed(pressed) {}
};

// a swing that hit nothing; cancelling keeps it from scripts
struct PlayerMissSwingBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;

    explicit PlayerMissSwingBeforeEvent(ServerPlayer &player) : mPlayer(player) {}
};

struct PlayerJumpAfterEvent : public Event {
    ServerPlayer &mPlayer;

    explicit PlayerJumpAfterEvent(ServerPlayer &player) : mPlayer(player) {}
};

// a queued move about to apply; cancelling sends the player back to mFrom
struct PlayerMoveBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Vector3f mFrom;
    Vector3f mTo;
    // the player is sent to mTo instead of moving there on their own
    bool mToChanged = false;

    PlayerMoveBeforeEvent(ServerPlayer &player, const Vector3f &from, const Vector3f &to)
            : mPlayer(player), mFrom(from), mTo(to) {}
};

// frost walker boots about to freeze the liquid around mCenter, one block below the feet
struct PlayerFrostWalkerBeforeEvent : public CancellableEvent {
    Level &mLevel;
    ServerPlayer &mPlayer;
    Vector3i mCenter;
    int32_t mRadius;
    BlockState &mLiquid;
    BlockState &mFrozen;
    // mLiquid must match exactly: else any still water freezes
    bool mExactLiquid = false;

    PlayerFrostWalkerBeforeEvent(Level &level, ServerPlayer &player, const Vector3i &center, int32_t radius,
                                 BlockState &liquid, BlockState &frozen)
            : mLevel(level), mPlayer(player), mCenter(center), mRadius(radius), mLiquid(liquid), mFrozen(frozen) {}
};

// a player about to destroy a block, with the drops and experience it gives; the game drops them itself after
struct PlayerDestroyBlockBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    const BlockState &mBlock;
    std::vector<ItemStack> &mDrops;
    int32_t &mExperience;

    PlayerDestroyBlockBeforeEvent(ServerPlayer &player, const Vector3i &position, const BlockState &block,
                                  std::vector<ItemStack> &drops, int32_t &experience)
            : mPlayer(player), mBlockPosition(position), mBlock(block), mDrops(drops), mExperience(experience) {}
};

struct PlayerInteractBlockBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    int32_t mFace;

    PlayerInteractBlockBeforeEvent(ServerPlayer &player, const Vector3i &position, int32_t face)
            : mPlayer(player), mBlockPosition(position), mFace(face) {}
};

struct PlayerPlaceBlockBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    const BlockState &mBlock;

    PlayerPlaceBlockBeforeEvent(ServerPlayer &player, const Vector3i &position, const BlockState &block)
            : mPlayer(player), mBlockPosition(position), mBlock(block) {}
};

struct PlayerChunkSentAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Level &mLevel;
    int32_t mChunkX;
    int32_t mChunkZ;

    PlayerChunkSentAfterEvent(ServerPlayer &player, Level &level, int32_t chunkX, int32_t chunkZ)
            : mPlayer(player), mLevel(level), mChunkX(chunkX), mChunkZ(chunkZ) {}
};

struct PlayerBeforeSignals {
    EventSignal<PlayerLoginAttemptBeforeEvent> mPlayerLoginAttempt;
    EventSignal<PlayerPreLoginBeforeEvent> mPlayerPreLogin;
    EventSignal<PlayerDuplicateLoginBeforeEvent> mPlayerDuplicateLogin;
    EventSignal<PlayerResourcePackOfferBeforeEvent> mPlayerResourcePackOffer;
    EventSignal<PlayerToggleBeforeEvent> mPlayerToggleSprint;
    EventSignal<PlayerToggleSneakBeforeEvent> mPlayerToggleSneak;
    EventSignal<PlayerToggleBeforeEvent> mPlayerToggleSwim;
    EventSignal<PlayerToggleBeforeEvent> mPlayerToggleGlide;
    EventSignal<PlayerToggleBeforeEvent> mPlayerToggleFlight;
    EventSignal<PlayerMissSwingBeforeEvent> mPlayerMissSwing;
    EventSignal<PlayerMoveBeforeEvent> mPlayerMove;
    EventSignal<PlayerFrostWalkerBeforeEvent> mPlayerFrostWalker;
    EventSignal<PlayerDestroyBlockBeforeEvent> mPlayerDestroyBlock;
    EventSignal<PlayerInteractBlockBeforeEvent> mPlayerInteractBlock;
    EventSignal<PlayerPlaceBlockBeforeEvent> mPlayerPlaceBlock;

    void clearPlayerBeforeSignals() {
        mPlayerLoginAttempt.clear();
        mPlayerPreLogin.clear();
        mPlayerDuplicateLogin.clear();
        mPlayerResourcePackOffer.clear();
        mPlayerToggleSprint.clear();
        mPlayerToggleSneak.clear();
        mPlayerToggleSwim.clear();
        mPlayerToggleGlide.clear();
        mPlayerToggleFlight.clear();
        mPlayerMissSwing.clear();
        mPlayerMove.clear();
        mPlayerFrostWalker.clear();
        mPlayerDestroyBlock.clear();
        mPlayerInteractBlock.clear();
        mPlayerPlaceBlock.clear();
    }
};

struct PlayerAfterSignals {
    EventSignal<PlayerCreationAfterEvent> mPlayerCreation;
    EventSignal<PlayerJumpAfterEvent> mPlayerJump;
    EventSignal<PlayerChunkSentAfterEvent> mPlayerChunkSent;

    void clearPlayerAfterSignals() {
        mPlayerCreation.clear();
        mPlayerJump.clear();
        mPlayerChunkSent.clear();
    }
};
