#include "Network/Handler/SubChunkRequestHandler.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockActorStore.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/SubChunkPacket.h"
#include "Protocol/Packets/SubChunkRequestPacket.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace {

    const int8_t HEIGHT_TOO_LOW = -1;
    const int8_t HEIGHT_TOO_HIGH = 16;

    std::string buildHeightMap(const std::vector<int32_t> &tops, int32_t baseY, HeightMapDataType &type) {
        bool allTooLow = true;
        bool allTooHigh = true;
        std::string values;
        values.resize(SubChunkData::HEIGHT_MAP_LENGTH, (char) (unsigned char) HEIGHT_TOO_LOW);

        for (size_t i = 0; i < tops.size(); i++) {
            const int32_t relative = tops[i] - baseY;
            int8_t stored;

            if (relative < 0) {
                stored = HEIGHT_TOO_LOW;
            } else if (relative > 15) {
                stored = HEIGHT_TOO_HIGH;
            } else {
                stored = (int8_t) relative;
            }

            if (stored != HEIGHT_TOO_LOW)
                allTooLow = false;
            if (stored != HEIGHT_TOO_HIGH)
                allTooHigh = false;

            values[i] = (char) (unsigned char) stored;
        }

        if (allTooLow) {
            type = HeightMapDataType::TooLow;
            return std::string();
        }

        if (allTooHigh) {
            type = HeightMapDataType::TooHigh;
            return std::string();
        }

        type = HeightMapDataType::HasData;
        return values;
    }

}

void SubChunkRequestHandler::handleRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                           const SubChunkRequestPacket &packet) {
    Level &level = owner.getLevelFor(player);
    const NetworkIdentifier &id = player.getNetworkIdentifier();
    const bool wrongDimension = packet.mDimension != level.getDimensionId();

    std::vector<SubChunkData> entries;
    entries.reserve(packet.mPositionOffsets.size());

    for (const Vector3i &offset: packet.mPositionOffsets) {
        SubChunkData entry;
        entry.mPosition = offset;

        if (wrongDimension) {
            entry.mResult = SubChunkRequestResult::InvalidDimension;
            entry.mHeightMapType = HeightMapDataType::NoData;
            entry.mRenderHeightMapType = entry.mHeightMapType;
            entries.push_back(entry);
            continue;
        }

        const int32_t chunkX = packet.mSubChunkPosition.x + offset.x;
        const int32_t subChunkY = packet.mSubChunkPosition.y + offset.y;
        const int32_t chunkZ = packet.mSubChunkPosition.z + offset.z;
        const int index = (int) (subChunkY - LevelChunk::LOWEST_SUB_CHUNK_Y);

        if (index < 0 || index >= LevelChunk::SUB_CHUNK_COUNT) {
            entry.mResult = SubChunkRequestResult::IndexOutOfBounds;
            entry.mHeightMapType = HeightMapDataType::NoData;
            entry.mRenderHeightMapType = entry.mHeightMapType;
            entries.push_back(entry);
            continue;
        }

        LevelChunk *chunk = level.isChunkPopulated(chunkX, chunkZ) ? level.peekChunkPtr(chunkX, chunkZ) : nullptr;
        if (chunk == nullptr) {
            entry.mResult = SubChunkRequestResult::ChunkNotFound;
            entry.mHeightMapType = HeightMapDataType::NoData;
            entry.mRenderHeightMapType = entry.mHeightMapType;
            entries.push_back(entry);
            continue;
        }

        const int32_t baseY = LevelChunk::MIN_Y + index * 16;
        entry.mHeightMapData = buildHeightMap(chunk->getTopBlockHeights(), baseY, entry.mHeightMapType);
        entry.mRenderHeightMapType = entry.mHeightMapType;
        entry.mRenderHeightMapData = entry.mHeightMapData;

        if (chunk->getSubChunk(index).isEmpty()) {
            entry.mResult = SubChunkRequestResult::SuccessAllAir;
            entries.push_back(entry);
            continue;
        }

        BlockActorStore &blockActors = level.getBlockActors();
        std::string blockEntities;
        if (blockActors.isChunkLoaded(chunkX, chunkZ))
            blockEntities = blockActors.encodeSubChunkNetwork(chunkX, subChunkY, chunkZ);
        else
            blockEntities = BlockActorStore::encodeNetworkForSection(level.loadBlockEntities(chunkX, chunkZ),
                                                                     subChunkY);

        entry.mResult = SubChunkRequestResult::Success;
        entry.mHasData = true;
        entry.mData = chunk->encodeSubChunkNetwork(index) + blockEntities;
        entries.push_back(entry);
    }

    for (size_t sent = 0; sent < entries.size(); sent += MAX_ENTRIES_PER_RESPONSE) {
        const size_t end = std::min(entries.size(), sent + (size_t) MAX_ENTRIES_PER_RESPONSE);

        SubChunkPacket response;
        response.mCacheEnabled = false;
        response.mDimension = packet.mDimension;
        response.mCenterPosition = packet.mSubChunkPosition;
        response.mSubChunks.assign(std::make_move_iterator(entries.begin() + (std::ptrdiff_t) sent),
                                   std::make_move_iterator(entries.begin() + (std::ptrdiff_t) end));

        owner.getNetworkHandler().send(id, response, owner.getCodecContext());
    }
}
