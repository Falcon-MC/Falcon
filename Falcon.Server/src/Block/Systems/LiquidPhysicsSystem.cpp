#include "Block/Systems/LiquidPhysicsSystem.h"

#include "Block/BlockData.h"
#include "Block/Blocks/LiquidView.h"
#include "Level/Level.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

void LiquidPhysicsSystem::moveStateFrom(LiquidPhysicsSystem &&other) {
    if (this == &other)
        return;
    mChanges = std::move(other.mChanges);
}

const BlockState &LiquidPhysicsSystem::_stateAt(int32_t x, int32_t y, int32_t z, int layer) {
    static const BlockState bedrock("minecraft:bedrock");
    static const BlockState air;

    const BlockState *state = mLevel.peekBlockPtr(x, y, z, layer);
    if (state == nullptr)
        return layer <= 0 ? bedrock : air;
    return *state;
}

int LiquidPhysicsSystem::_fluidLayer(int32_t x, int32_t y, int32_t z) {
    const BlockState &layer0 = _stateAt(x, y, z);
    if (isFluidState(layer0))
        return 0;

    if (getWaterloggingLevel(layer0) == 0)
        return -1;

    return LiquidView(_stateAt(x, y, z, 1)).isWater() ? 1 : -1;
}

const BlockState &LiquidPhysicsSystem::_fluidAt(int32_t x, int32_t y, int32_t z) {
    return _fluidLayer(x, y, z) == 1 ? _stateAt(x, y, z, 1) : _stateAt(x, y, z);
}

const BlockState &LiquidPhysicsSystem::fluidStateAt(const LevelChunk &chunk, int32_t localX, int32_t y,
                                                    int32_t localZ) {
    const BlockState &layer0 = chunk.getBlock(localX, y, localZ);
    const LiquidView liquid(layer0);
    if (liquid.isLiquid() || liquid.isBubbleColumn() || getWaterloggingLevel(layer0) == 0)
        return layer0;

    const BlockState &layer1 = chunk.getBlock(localX, y, localZ, 1);
    return LiquidView(layer1).isWater() ? layer1 : layer0;
}

uint8_t LiquidPhysicsSystem::getWaterloggingLevel(const BlockState &state) {
    if (state.mName == "minecraft:air")
        return 0;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data == nullptr ? 0 : data->mWaterloggingLevel;
}

bool LiquidPhysicsSystem::_isLoaded(int32_t x, int32_t z) const {
    return mLevel.isChunkResident(x >> 4, z >> 4);
}

void LiquidPhysicsSystem::onBlockChanged(int32_t x, int32_t y, int32_t z) {
    scheduleNeighbors(x, y, z);
}

LiquidInfo LiquidPhysicsSystem::getLiquidInfo(int32_t x, int32_t y, int32_t z) {
    LiquidInfo result;
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return result;

    const LiquidView block(_fluidAt(x, y, z));
    result.water = block.isWater();
    result.lava = block.isLava();
    result.bubble = block.isBubbleColumn();
    result.dragDown = result.bubble && block.isDragDown();
    result.source = block.isSource();
    result.falling = block.isFalling();
    result.decay = block.getDecay();
    result.height = block.isLiquid() || result.bubble ? block.getFluidHeightPercent() : 0.0f;
    return result;
}

Vector3f LiquidPhysicsSystem::getFlowVector(const Vector3i &position) {
    const LiquidView liquid(_fluidAt(position.x, position.y, position.z));
    if (!liquid.isLiquid())
        return Vector3f();

    const bool lava = liquid.isLava();
    const int decay = liquid.isSource() || liquid.isFalling() ? 0 : liquid.getDecay();
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    static const int offsets[4][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    for (const auto &offset: offsets) {
        const int32_t nx = position.x + offset[0];
        const int32_t ny = position.y;
        const int32_t nz = position.z + offset[2];
        const BlockState &side = _fluidAt(nx, ny, nz);
        const LiquidView sideLiquid(side);
        if (sideLiquid.isLiquid() && sideLiquid.isLava() == lava) {
            const int sideDecay = sideLiquid.isSource() || sideLiquid.isFalling() ? 0 : sideLiquid.getDecay();
            const int realDecay = sideDecay - decay;
            x += (float) offset[0] * (float) realDecay;
            z += (float) offset[2] * (float) realDecay;
        } else if (isFlowable(side, lava)) {
            const BlockState &below = _fluidAt(nx, ny - 1, nz);
            const LiquidView belowLiquid(below);
            if (belowLiquid.isLiquid() && belowLiquid.isLava() == lava) {
                const int belowDecay = belowLiquid.isSource() || belowLiquid.isFalling() ? 0 : belowLiquid.getDecay();
                const int realDecay = belowDecay - (decay - 8);
                x += (float) offset[0] * (float) realDecay;
                z += (float) offset[2] * (float) realDecay;
            }
        }
    }

    if (liquid.isFalling()) {
        for (const auto &offset: offsets) {
            const BlockState side = _fluidAt(position.x + offset[0], position.y, position.z + offset[2]);
            const BlockState above = _fluidAt(position.x + offset[0], position.y + 1, position.z + offset[2]);
            if (!isFlowable(side, lava) || !isFlowable(above, lava)) {
                const float length = std::sqrt(x * x + y * y + z * z);
                if (length > 0.0f) {
                    x /= length;
                    z /= length;
                }
                y -= 6.0f;
                break;
            }
        }
    }

    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.0f)
        return Vector3f();
    return Vector3f(x / length, y / length, z / length);
}

void LiquidPhysicsSystem::schedule(const Vector3i &position, int64_t delay) {
    mLevel.getBlockUpdateScheduler().schedule(position, delay);
}

void LiquidPhysicsSystem::onScheduledUpdate(const Vector3i &position) {
    process(position);
}

std::vector<LiquidChange> LiquidPhysicsSystem::consumeChanges() {
    std::vector<LiquidChange> result;
    result.swap(mChanges);
    return result;
}

bool LiquidPhysicsSystem::isFluidState(const BlockState &state) const {
    return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water"
           || state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava"
           || state.mName == "minecraft:bubble_column";
}

bool LiquidPhysicsSystem::isSameFluid(const BlockState &left, const BlockState &right) const {
    const bool leftWater = left.mName == "minecraft:water" || left.mName == "minecraft:flowing_water"
                           || left.mName == "minecraft:bubble_column";
    const bool rightWater = right.mName == "minecraft:water" || right.mName == "minecraft:flowing_water"
                            || right.mName == "minecraft:bubble_column";
    const bool leftLava = left.mName == "minecraft:lava" || left.mName == "minecraft:flowing_lava";
    const bool rightLava = right.mName == "minecraft:lava" || right.mName == "minecraft:flowing_lava";
    return (leftWater && rightWater) || (leftLava && rightLava);
}

bool LiquidPhysicsSystem::isFlowable(const BlockState &state, bool lava) const {
    if (state.mName == "minecraft:air" || isFluidState(state))
        return true;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    if (!lava && data->mWaterloggingLevel > 1)
        return true;

    if (!lava && data->mWaterloggingLevel == 1)
        return false;

    return !data->mSolid;
}

int64_t LiquidPhysicsSystem::getTickRate(const BlockState &state) const {
    const LiquidView liquid(state);
    if (liquid.isLiquid() || liquid.isBubbleColumn())
        return liquid.getTickRate();
    return 1;
}

void LiquidPhysicsSystem::scheduleNeighbors(int32_t x, int32_t y, int32_t z) {
    static const int offsets[7][3] = {
            {0, 0, 0}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}
    };

    for (const auto &offset: offsets) {
        const Vector3i position(x + offset[0], y + offset[1], z + offset[2]);
        const BlockState state = _fluidAt(position.x, position.y, position.z);
        const LiquidView liquid(state);
        if (liquid.isLiquid() || liquid.isBubbleColumn())
            schedule(position, getTickRate(state));
    }
}

BlockState LiquidPhysicsSystem::makeState(bool lava, int decay, bool falling) const {
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", std::clamp(decay, 0, 7));
    const bool source = !falling && decay == 0;
    const std::string name = lava
                             ? (source ? "minecraft:lava" : "minecraft:flowing_lava")
                             : (source ? "minecraft:water" : "minecraft:flowing_water");
    return BlockState(name, states);
}

void LiquidPhysicsSystem::_writeLayer(const Vector3i &position, int layer, const BlockState &state) {
    if (_stateAt(position.x, position.y, position.z, layer) == state)
        return;

    mChanges.push_back(LiquidChange{position, state, layer});
    if (layer == 0)
        mLevel.setBlockState(position.x, position.y, position.z, state);
    else
        mLevel.setBlockStateAtLayer(position.x, position.y, position.z, layer, state);
}

void LiquidPhysicsSystem::setFluidState(const Vector3i &position, const BlockState &state) {
    if (!_isLoaded(position.x, position.z))
        return;

    const BlockState layer0 = _stateAt(position.x, position.y, position.z);
    const bool waterlogged = _fluidLayer(position.x, position.y, position.z) == 1;
    const LiquidView liquid(state);

    if (!isFluidState(state) && state.mName != "minecraft:air") {
        if (waterlogged)
            _writeLayer(position, 1, BlockState());
        _writeLayer(position, 0, state);
        return;
    }

    const uint8_t waterlogging = getWaterloggingLevel(layer0);
    const bool intoLayer1 = waterlogged
                            || (liquid.isWater() && !isFluidState(layer0) && waterlogging > 0);
    if (!intoLayer1) {
        _writeLayer(position, 0, state);
        return;
    }

    if (liquid.isWater() && waterlogging == 1 && !liquid.isSource()) {
        _writeLayer(position, 1, BlockState());
        return;
    }

    _writeLayer(position, 1, state);
}

void LiquidPhysicsSystem::normalizeWaterlogged(const Vector3i &position) {
    const BlockState layer1 = _stateAt(position.x, position.y, position.z, 1);
    const LiquidView liquid(layer1);
    if (!liquid.isLiquid())
        return;

    const BlockState layer0 = _stateAt(position.x, position.y, position.z);
    if (layer0.mName == "minecraft:air") {
        _writeLayer(position, 1, BlockState());
        _writeLayer(position, 0, layer1);
        return;
    }

    const uint8_t waterlogging = getWaterloggingLevel(layer0);
    if (waterlogging == 0 || (waterlogging == 1 && !liquid.isSource()))
        _writeLayer(position, 1, BlockState());
}

void LiquidPhysicsSystem::harden(const Vector3i &position) {
    const BlockState current = _fluidAt(position.x, position.y, position.z);
    const LiquidView liquid(current);
    if (!liquid.isLava() || liquid.isFalling())
        return;

    static const int offsets[6][3] = {
            {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}
    };
    for (const auto &offset: offsets) {
        const Vector3i neighbor(position.x + offset[0], position.y + offset[1], position.z + offset[2]);
        const BlockState otherState = _fluidAt(neighbor.x, neighbor.y, neighbor.z);
        const LiquidView other(otherState);
        if (!other.isLiquid() || other.isWater() == liquid.isWater())
            continue;

        const std::string result = liquid.isSource() ? "minecraft:obsidian"
                              : liquid.getDecay() <= 4 ? "minecraft:cobblestone" : "minecraft:stone";
        setFluidState(position, BlockState(result));
        return;
    }
}

bool LiquidPhysicsSystem::resolveFluidCollision(const Vector3i &target, const BlockState &sourceState,
                                                bool downward) {
    if (target.y < LevelChunk::MIN_Y || target.y > LevelChunk::MAX_Y)
        return false;

    const BlockState targetState = _fluidAt(target.x, target.y, target.z);
    if (!isFluidState(targetState) || !isFluidState(sourceState))
        return false;
    if (isSameFluid(targetState, sourceState))
        return false;

    const LiquidView targetLiquid(targetState);
    std::string result;

    if (targetLiquid.isLava()) {
        result = targetLiquid.isSource() ? "minecraft:obsidian"
               : targetLiquid.getDecay() <= 4 ? "minecraft:cobblestone"
               : "minecraft:stone";
    } else {
        if (_fluidLayer(target.x, target.y, target.z) == 1)
            return true;
        result = downward ? "minecraft:stone" : "minecraft:cobblestone";
    }

    setFluidState(target, BlockState(result));
    return true;
}

void LiquidPhysicsSystem::processBubbleColumn(const Vector3i &position) {
    const BlockState state = _stateAt(position.x, position.y, position.z);
    const LiquidView column(state);
    if (!column.isBubbleColumn())
        return;

    const BlockState below = _stateAt(position.x, position.y - 1, position.z);
    const bool supported = below.mName == "minecraft:magma_block" ? column.isDragDown()
                         : below.mName == "minecraft:soul_sand" ? !column.isDragDown()
                         : LiquidView(below).isBubbleColumn()
                           && LiquidView(below).isDragDown() == column.isDragDown();
    if (!supported) {
        setFluidState(position, makeState(false, 0, false));
        return;
    }

    const BlockState above = _stateAt(position.x, position.y + 1, position.z);
    const LiquidView aboveLiquid(above);
    if (aboveLiquid.isSource() && aboveLiquid.isWater()) {
        Tag states = Tag::ofCompound();
        states.putByte("drag_down", column.isDragDown() ? 1 : 0);
        setFluidState(Vector3i(position.x, position.y + 1, position.z),
                      BlockState("minecraft:bubble_column", states));
    }
    schedule(position, 5);
}

void LiquidPhysicsSystem::process(const Vector3i &position) {
    if (!_isLoaded(position.x, position.z))
        return;

    normalizeWaterlogged(position);

    const BlockState state = _fluidAt(position.x, position.y, position.z);
    const LiquidView liquid(state);
    if (liquid.isBubbleColumn()) {
        processBubbleColumn(position);
        return;
    }
    if (!liquid.isLiquid())
        return;

    harden(position);
    const BlockState currentState = _fluidAt(position.x, position.y, position.z);
    const LiquidView current(currentState);
    if (!current.isLiquid())
        return;

    const bool waterlogged = _fluidLayer(position.x, position.y, position.z) == 1;

    if (current.isWater() && current.isSource() && !waterlogged) {
        const BlockState below = _stateAt(position.x, position.y - 1, position.z);
        if (below.mName == "minecraft:magma_block" || below.mName == "minecraft:soul_sand") {
            Tag states = Tag::ofCompound();
            states.putByte("drag_down", below.mName == "minecraft:magma_block" ? 1 : 0);
            setFluidState(position, BlockState("minecraft:bubble_column", states));
            return;
        }
    }

    const bool source = current.isSource();
    const bool lava = current.isLava();
    const int step = current.getFlowDecayPerBlock();
    const Vector3i belowPosition(position.x, position.y - 1, position.z);
    resolveFluidCollision(belowPosition, currentState, true);

    const BlockState below = _fluidAt(belowPosition.x, belowPosition.y, belowPosition.z);
    const bool belowFlowable = isFlowable(below, lava) && !isSameFluid(below, currentState);
    if (belowFlowable) {
        setFluidState(belowPosition, makeState(lava, 0, true));
        if (!source)
            schedule(position, current.getTickRate());
        return;
    }

    const bool belowSameFluid = isSameFluid(below, currentState);
    const int baseDecay = source || current.isFalling() ? 0 : current.getDecay();
    const int nextDecay = baseDecay + step;
    if (nextDecay <= 7 && (source || !belowSameFluid)) {
        static const int offsets[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};

        for (int j = 0; j < 4; ++j) {
            const Vector3i side(position.x + offsets[j][0], position.y, position.z + offsets[j][2]);
            if (resolveFluidCollision(side, currentState, false))
                continue;

            const BlockState sideState = _fluidAt(side.x, side.y, side.z);
            if (!isFlowable(sideState, lava) || (isSameFluid(sideState, currentState) && LiquidView(sideState).isSource()))
                continue;

            if (isSameFluid(sideState, currentState)) {
                const LiquidView sideLiquid(sideState);
                const int sideDecay = sideLiquid.isFalling() ? 0 : sideLiquid.getDecay();
                if (sideDecay <= nextDecay)
                    continue;
            }
            setFluidState(side, makeState(lava, nextDecay, false));
        }
    }

    if (!source) {
        if (current.isWater()) {
            int sources = 0;
            static const int sourceOffsets[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
            for (const auto &offset: sourceOffsets) {
                const LiquidView side(_fluidAt(position.x + offset[0], position.y, position.z + offset[2]));
                if (side.isWater() && side.isSource())
                    ++sources;
            }
            const LiquidView belowLiquid(below);
            const BlockData *belowData = BlockDataTable::find(
                    _stateAt(belowPosition.x, belowPosition.y, belowPosition.z).mName.c_str());
            const int minSources = current.getMinAdjacentSourcesToFormSource();
            if (sources >= minSources && ((belowData != nullptr && belowData->mSolid)
                                 || (belowLiquid.isWater() && belowLiquid.isSource()))) {
                setFluidState(position, makeState(false, 0, false));
                return;
            }
        }

        const BlockState above = _fluidAt(position.x, position.y + 1, position.z);
        if (isSameFluid(above, currentState)) {
            if (!current.isFalling())
                setFluidState(position, makeState(lava, 0, true));
            return;
        }

        int smallest = std::numeric_limits<int>::max();
        static const int offsets[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
        for (const auto &offset: offsets) {
            const BlockState side = _fluidAt(position.x + offset[0], position.y, position.z + offset[2]);
            if (!isSameFluid(side, currentState))
                continue;
            const LiquidView sideLiquid(side);
            const int sideDecay = sideLiquid.isSource() || sideLiquid.isFalling() ? 0 : sideLiquid.getDecay();
            smallest = std::min(smallest, sideDecay);
        }

        if (smallest == std::numeric_limits<int>::max() || smallest + step > 7) {
            setFluidState(position, BlockState("minecraft:air"));
            return;
        }

        const int newDecay = smallest + step;
        if (newDecay != current.getDecay() || current.isFalling())
            setFluidState(position, makeState(lava, newDecay, false));
    }
}

bool LiquidPhysicsSystem::_canBeFlowedInto(const BlockState &state, bool lava) const {
    if (isFluidState(state) && LiquidView(state).isSource())
        return false;
    return isFlowable(state, lava);
}

int LiquidPhysicsSystem::_calculateFlowCost(int32_t x, int32_t y, int32_t z, int accumulatedCost, int maxCost,
                                            int originOpposite, int lastOpposite, bool lava) {
    std::unordered_map<Position, int8_t, PositionHash> &visited = mFlowCostVisited;

    static const int offsets[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};

    int cost = 1000;
    for (int j = 0; j < 4; ++j) {
        if (j == originOpposite || j == lastOpposite)
            continue;

        const int32_t nextX = x + offsets[j][0];
        const int32_t nextZ = z + offsets[j][2];
        const Position key{nextX, y, nextZ};

        int8_t status;
        auto found = visited.find(key);
        if (found != visited.end()) {
            status = found->second;
        } else {
            const BlockState side = _fluidAt(nextX, y, nextZ);
            if (!_canBeFlowedInto(side, lava)) {
                status = FLOW_BLOCKED;
            } else {
                const BlockState under = _fluidAt(nextX, y - 1, nextZ);
                status = _canBeFlowedInto(under, lava) ? FLOW_CAN_FLOW_DOWN : FLOW_CAN_FLOW;
            }
            visited.emplace(key, status);
        }

        if (status == FLOW_BLOCKED)
            continue;
        if (status == FLOW_CAN_FLOW_DOWN)
            return accumulatedCost;
        if (accumulatedCost >= maxCost)
            continue;

        const int realCost = _calculateFlowCost(nextX, y, nextZ, accumulatedCost + 1, maxCost,
                                                originOpposite, j ^ 0x01, lava);
        if (realCost < cost)
            cost = realCost;
    }

    return cost;
}

void LiquidPhysicsSystem::_getOptimalFlowDirections(int32_t x, int32_t y, int32_t z, int decayPerBlock, bool lava,
                                                    bool out[4]) {
    static const int offsets[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};

    int flowCost[4] = {1000, 1000, 1000, 1000};
    int maxCost = 4 / decayPerBlock;
    std::unordered_map<Position, int8_t, PositionHash> &visited = mFlowCostVisited;
    visited.clear();

    for (int j = 0; j < 4; ++j) {
        const int32_t nextX = x + offsets[j][0];
        const int32_t nextZ = z + offsets[j][2];
        const Position key{nextX, y, nextZ};
        const BlockState side = _fluidAt(nextX, y, nextZ);

        if (!_canBeFlowedInto(side, lava)) {
            visited[key] = FLOW_BLOCKED;
        } else {
            const BlockState under = _fluidAt(nextX, y - 1, nextZ);
            if (_canBeFlowedInto(under, lava)) {
                visited[key] = FLOW_CAN_FLOW_DOWN;
                flowCost[j] = 0;
                maxCost = 0;
            } else if (maxCost > 0) {
                visited[key] = FLOW_CAN_FLOW;
                flowCost[j] = _calculateFlowCost(nextX, y, nextZ, 1, maxCost, j ^ 0x01, j ^ 0x01, lava);
                maxCost = std::min(maxCost, flowCost[j]);
            }
        }
    }

    int minCost = 1000;
    for (int j = 0; j < 4; ++j)
        minCost = std::min(minCost, flowCost[j]);

    for (int j = 0; j < 4; ++j)
        out[j] = flowCost[j] == minCost;
}
