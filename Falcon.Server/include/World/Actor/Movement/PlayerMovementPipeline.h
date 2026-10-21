#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

struct PlayerMovementContext;

/** ECS system schedule shared by live inputs and catch-up replay. */
class PlayerMovementSystemRegistry {
public:
    enum class Phase : uint8_t { Admission, PreTravel, Travel, PostTravel, Prediction };
    using System = bool (*)(PlayerMovementContext &);

    void registerSystem(Phase phase, std::string_view name, System system);
    void run(PlayerMovementContext &context) const;

private:
    struct Entry {
        Phase phase;
        std::string_view name;
        System system;
    };
    std::vector<Entry> mSystems;
};

/** Builds and executes the same registered ECS schedule for live and replayed inputs. */
class PlayerMovementPipeline {
public:
    static void tick(PlayerMovementContext &context);
};
