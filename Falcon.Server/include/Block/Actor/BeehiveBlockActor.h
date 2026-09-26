#pragma once

#include "Block/BlockActor.h"

#include <cstdint>
#include <string>
#include <vector>

class Level;
class MobActor;
class ServerNetworkHandler;

class BeehiveBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Beehive";

    static constexpr int32_t MAX_OCCUPANTS = 3;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool isFull() const;

    bool isEmpty() const;

    void addOccupant(const std::string &actorIdentifier, Tag saveData, int32_t ticksLeftToStay);

    void clearOccupants();

    bool admit(ServerNetworkHandler &owner, MobActor &mob);

    void evacuate(ServerNetworkHandler &owner, bool hiveRemains);

    bool tick(ServerNetworkHandler &owner) override;

private:
    struct Occupant {
        std::string mActorIdentifier;
        Tag mSaveData;
        int32_t mTicksLeftToStay = 0;
    };

    std::vector<int> _spawnFaces(Level &level, int frontFace, bool vertical) const;

    bool _isFireNearby(Level &level) const;

    bool _release(ServerNetworkHandler &owner, Level &level, const Occupant &occupant, int face,
                  const std::string &event, bool deliverNectar);

    std::vector<Occupant> mOccupants;
};
