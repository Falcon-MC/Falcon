#pragma once

#include "Core/NBT/Tag.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Block/BlockState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct BlockData;
class Actor;
class BlockBehavior;
class ItemStack;
class Level;
class ServerActor;
class ServerNetworkHandler;
class ServerPlayer;

struct BlockPlacementContext {
    Level *mLevel;
    float mYaw;
    float mPitch;
    int mFace;
    Vector3f mClickPosition;
    Vector3f mPlayerPosition;
    Vector3i mBlockPosition;
    int mPlayerFacing;
    int mOppositeFacing;
    int mOrdinal;
    int mPistonFacing;
};

struct BlockPlacementEntry {
    Vector3i mPosition;
    BlockState mState;
};

struct BlockDrop {
    std::string mIdentifier;
    int32_t mCount;
};

enum class PlacementMergeResult {
    None,
    Merged,
    Rejected
};

enum class PistonMoveReaction {
    Normal,
    Break,
    PushOnly
};

class Block {
public:
    virtual ~Block() = default;

    virtual bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state) const {
        (void) owner;
        (void) player;
        (void) position;
        (void) state;
        return false;
    }

    virtual void onTouch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state) const {
        (void) owner;
        (void) player;
        (void) position;
        (void) state;
    }

    virtual void onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                          const BlockState &state) const {
        (void) owner;
        (void) actor;
        (void) position;
        (void) state;
    }

    virtual void onFallOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                          const BlockState &state, float fallDistance) const {
        (void) owner;
        (void) actor;
        (void) position;
        (void) state;
        (void) fallDistance;
    }

    virtual bool onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state) const {
        (void) owner;
        (void) player;
        (void) position;
        (void) state;
        return false;
    }

    virtual bool canBeReplaced(const BlockState &state) const {
        (void) state;
        return false;
    }

    virtual bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
        (void) level;
        (void) position;
        (void) blockFace;
        return true;
    }

    virtual Vector3i resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const {
        (void) level;
        (void) blockFace;
        return position;
    }

    virtual PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                const Vector3f &clickPosition, Vector3i &position,
                                                BlockState &state) const {
        (void) level;
        (void) clickedPosition;
        (void) blockFace;
        (void) clickPosition;
        (void) position;
        (void) state;
        return PlacementMergeResult::None;
    }

    virtual void onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           BlockState &state) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
    }

    virtual void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                          const BlockState &state, const ItemStack &usedItem, int blockFace) const {
        (void) owner;
        (void) player;
        (void) position;
        (void) state;
        (void) usedItem;
        (void) blockFace;
    }

    virtual void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                          const BlockState &state) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
    }

    virtual void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                               const BlockState &state) const {
        (void) owner;
        (void) actor;
        (void) position;
        (void) state;
    }

    virtual void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
    }

    virtual void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
    }

    virtual std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                                const BlockState &state, int playerFacing) const;

    virtual std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                                    const BlockState &state) const;

    virtual bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const;

    virtual void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const;

    virtual bool onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, ServerActor &projectile) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
        (void) projectile;
        return false;
    }

    virtual void writeDropContents(Level &level, const Vector3i &position, ItemStack &drop) const {
        (void) level;
        (void) position;
        (void) drop;
    }

    virtual bool getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
        (void) state;
        (void) shape;
        return false;
    }

    virtual std::string getResourceItem(const BlockState &state) const {
        return state.mName;
    }

    virtual int32_t getResourceCount(const BlockState &state) const {
        (void) state;
        return 1;
    }

    virtual bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                          std::vector<BlockDrop> &drops) const {
        (void) state;
        (void) tool;
        (void) fortuneLevel;
        (void) drops;
        return false;
    }

    static bool isShears(const ItemStack &tool);

    static std::vector<BlockDrop> grassDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                             int32_t seedChance);

    virtual bool isSignalSource() const {
        return false;
    }

    virtual PistonMoveReaction getPistonMoveReaction() const {
        return PistonMoveReaction::Normal;
    }

    virtual BlockState applyPlacementOrientation(const BlockState &state,
                                                 const BlockPlacementContext &context) const;

    Block() : mTypeId(0), mIdentifier("minecraft:air"), mName("Air"), mStates(Tag::ofCompound()) {}

    Block(int32_t typeId, const std::string &identifier, const std::string &name)
            : mTypeId(typeId), mIdentifier(identifier), mName(name), mStates(Tag::ofCompound()) {}

    Block(int32_t typeId, const std::string &identifier, const std::string &name, const Tag &states)
            : mTypeId(typeId), mIdentifier(identifier), mName(name), mStates(states) {}

    explicit Block(const BlockState &state)
            : mTypeId(0), mIdentifier(state.mName), mName(state.mName), mStates(state.mStates) {}

    int32_t getTypeId() const { return mTypeId; }

    const std::string &getIdentifier() const { return mIdentifier; }

    const std::string &getName() const { return mName; }

    const Tag &getStates() const { return mStates; }

    const BlockData *getData() const;

    const BlockBehavior &getBehavior() const;

    float getFrictionFactor() const;

    bool onEntityLand(Actor &actor, float downwardVelocity) const;

    std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const;

    BlockState toBlockState() const { return BlockState(mIdentifier, mStates); }

    int32_t getNetworkHash() const { return BlockStateHasher::hash(mIdentifier, mStates); }

private:
    int32_t mTypeId;
    std::string mIdentifier;
    std::string mName;
    Tag mStates;
};
