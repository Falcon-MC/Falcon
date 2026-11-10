#pragma once

#include <functional>

#include "Core/NBT/Tag.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct BlockData;
class Actor;
class BlockBehavior;
class ItemStack;
class Level;
class LiquidPhysicsSystem;
class MobActor;
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

enum class PlacementMergeResult { None, Merged, Rejected };

enum class BlockTraversal { None, Scaffolding, PowderSnow };

enum class PathHazard { None, Damaging, Portal };

enum class PistonMoveReaction { Normal, Break, PushOnly };

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

    virtual void onFallOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position, const BlockState &state,
                          float fallDistance) const {
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

    // faces liquid cannot cross, a bit per LiquidFace (BlockType +185, stops_liquid_flowing_from_direction)
    virtual uint8_t getLiquidBlockedFaces(const BlockState &state) const {
        (void) state;
        return 0;
    }

    // Material type 18: liquid falling onto it turns it to stone (sub_149C3E1F0)
    virtual bool isIceMaterial() const {
        return false;
    }

    // true for blocks reacting to the hooks below: other blocks never reach them
    virtual bool usesBlockTicks() const {
        return false;
    }

    // BlockPlaceEvent, neighborChanged (slot 75) and BlockQueuedTickEvent as the tick queues run them;
    // `ticks` reads and writes the world and queues ticks
    virtual void onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
        (void) ticks;
        (void) position;
        (void) state;
    }

    virtual void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                        const Vector3i &changed) const {
        (void) ticks;
        (void) position;
        (void) state;
        (void) changed;
    }

    virtual void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
        (void) ticks;
        (void) position;
        (void) state;
    }

    virtual bool canBeReplaced(const BlockState &state) const {
        (void) state;
        return false;
    }

    // replaceable, yet stays when an actor item (armor stand) is placed into it
    virtual bool staysUnderPlacedActor() const {
        return false;
    }

    // head slot when given to an armor stand
    virtual bool isWornOnHead() const {
        return false;
    }

    virtual bool isClimbable() const {
        return false;
    }

    virtual bool isLeaves() const {
        return false;
    }

    virtual bool suppressesFreezing() const {
        return false;
    }

    virtual bool freezesActorsInside() const {
        return false;
    }

    virtual float getSpeedFactor() const {
        return 1.0f;
    }

    virtual float getJumpFactor() const {
        return 1.0f;
    }

    virtual float getAccelerationFrictionMultiplier() const {
        return 1.0f;
    }

    virtual BlockTraversal getTraversal() const {
        return BlockTraversal::None;
    }

    // navigation options decide whether the hazard is avoided
    virtual PathHazard getPathHazard() const {
        return PathHazard::None;
    }

    virtual bool getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const {
        (void) actor;
        (void) multiplier;
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

    virtual bool onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state, const std::string &event, MobActor &source) const {
        (void) owner;
        (void) level;
        (void) position;
        (void) state;
        (void) event;
        (void) source;
        return false;
    }

    virtual bool bindsHomeActors() const {
        return false;
    }

    // true in every class overriding onRandomTick: random ticks skip inert blocks
    virtual bool hasRandomTick() const {
        return false;
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

    virtual bool hasDynamicShape() const {
        return false;
    }

    virtual BlockState updateShape(Level &level, const Vector3i &position, const BlockState &state) const {
        (void) level;
        (void) position;
        return state;
    }

    virtual void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const;

    // whether a normal update from `changed` reaches onNeighbourChanged; null `changed` is the block itself
    virtual bool watchesNeighbour(const Vector3i &position, const BlockState &state, const Vector3i *changed) const {
        (void) position;
        (void) state;
        (void) changed;
        return true;
    }

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

    // item and projectile physics read the shape only of these; other blocks stop them at the cell
    virtual bool shapesCollisionPerActor() const {
        return false;
    }

    // several boxes resolved with the neighbours; false leaves the single shape and the vanilla table
    virtual bool getCollisionShapesAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                                      const std::function<const BlockState *(int32_t, int32_t, int32_t)> &neighbours,
                                      std::vector<AxisAlignedBB> &shapes) const {
        (void) state;
        (void) position;
        (void) actor;
        (void) neighbours;
        (void) shapes;
        return false;
    }

    // actor null outside a move: placement, explosions, block rays
    // shapes that depend on where the actor stands against the block
    virtual bool getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                                     AxisAlignedBB &shape) const {
        (void) position;
        return getCollisionShape(state, actor, shape);
    }

    virtual bool getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
        (void) state;
        (void) actor;
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

    static bool hasSilkTouch(const ItemStack &tool);

    static int32_t multifaceCount(const BlockState &state);

    static std::vector<BlockDrop> grassDrops(const std::string &shearedItem, int32_t shearedCount,
                                             const ItemStack &tool);

    virtual bool isSignalSource() const {
        return false;
    }

    // block light of a state; `dataEmission` is the value the block data gives every state
    virtual int getLightEmission(const BlockState &state, int dataEmission) const {
        (void) state;
        return dataEmission;
    }

    virtual PistonMoveReaction getPistonMoveReaction() const {
        return PistonMoveReaction::Normal;
    }

    virtual BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const;

    Block() : mTypeId(0), mIdentifier("minecraft:air"), mName("Air"), mStates(Tag::ofCompound()) {}

    Block(int32_t typeId, const std::string &identifier, const std::string &name)
            : mTypeId(typeId), mIdentifier(identifier), mName(name), mStates(Tag::ofCompound()) {}

    Block(int32_t typeId, const std::string &identifier, const std::string &name, const Tag &states)
            : mTypeId(typeId), mIdentifier(identifier), mName(name), mStates(states) {}

    explicit Block(const BlockState &state)
            : mTypeId(0), mIdentifier(state.mName), mName(state.mName), mStates(state.mStates) {}

    int32_t getTypeId() const {
        return mTypeId;
    }

    const std::string &getIdentifier() const {
        return mIdentifier;
    }

    const std::string &getName() const {
        return mName;
    }

    const Tag &getStates() const {
        return mStates;
    }

    const BlockData *getData() const;

    const BlockBehavior &getBehavior() const;

    float getFrictionFactor() const;

    bool onEntityLand(Actor &actor, float downwardVelocity) const;

    std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const;

    float getFallDistanceMultiplier() const;

    BlockState toBlockState() const {
        return BlockState(mIdentifier, mStates);
    }

    int32_t getNetworkHash() const {
        return BlockStateHasher::hash(mIdentifier, mStates);
    }

private:
    int32_t mTypeId;
    std::string mIdentifier;
    std::string mName;
    Tag mStates;
};
