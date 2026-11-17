#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/VaultBlockActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Level.h"

namespace {
    class VaultBlock final : public Block {
    public:
        explicit VaultBlock(const Block &block) : Block(block) {}

        static bool matches(const std::string &identifier) {
            return identifier == "minecraft:vault";
        }

        const char *getBlockActorId() const override {
            return VaultBlockActor::BLOCK_ACTOR_ID;
        }

        void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                      const BlockState &, const ItemStack &, int) const override {
            owner.getLevelFor(player).getBlockActors().getOrCreate<VaultBlockActor>(position);
        }

        bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                        const BlockState &) const override {
            auto &vault = owner.getLevelFor(player).getBlockActors().getOrCreate<VaultBlockActor>(position);
            vault.unlock(owner, player);
            return true;
        }
    };

    FALCON_REGISTER_BLOCK(VaultBlock, 200);
}
