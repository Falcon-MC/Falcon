# Falcon server source layout

The `include/` and `src/` trees follow BDS's `handheld/src/common/` domains,
using PascalCase directories to satisfy Falcon's repository rules. The separate `Core/` module and the public
`Falcon/EmbeddedServer.h` embedding API keep their existing names.

This is a file organization refactor. C++ class names, inheritance, responsibilities,
serialization, protocol and gameplay are unchanged. It does not reproduce BDS's ECS,
class layouts, ABI or complete private source tree.

## Reference and evidence

Reference: **Bedrock Dedicated Server 1.26.10 for Windows**, image base `0x140000000`.
Evidence was read from the IDA database's source-path assertion strings and C++ type
names. No BDS implementation code is included.

| BDS assertion source path, relative to `handheld/src/common/` | String address |
|---|---|
| `network/ServerNetworkHandler.cpp` | `0x1468c6300` |
| `server/ServerInstance.cpp` | `0x1469aab28` |
| `world/actor/player/Player.cpp` | `0x146a539b0` |
| `world/level/Level.cpp` | `0x146ab3838` |
| `world/level/chunk/LevelChunk.cpp` | `0x146abd6e0` |
| `world/level/chunk/SubChunk.cpp` | `0x146ad00b0` |
| `world/level/dimension/Dimension.cpp` | `0x146abdee0` |
| `world/level/block/BlockType.cpp` | `0x146abb1f0` |
| `resources/ResourcePackManager.cpp` | `0x146933590` |

The machine-readable [inventory](bds-1.26.10.json) records all 1,776 original server
source/header files, their new locations, original content hashes and available BDS
source/type-name addresses. It is a migration snapshot, not a list of identical BDS
implementations. Its `baseline` field identifies the Falcon commit before the move.

The binary exposes only a sparse subset of source paths. Directly observed paths
are distinguished by `bds_source` entries. For other files, the domain is inferred
from the existing Falcon responsibility; child folders generally retain Falcon's
organization with PascalCase directory names. `Server/Commands`, AI subdivisions,
block systems, loot and `Falcon/Plugin` are organizational choices, not claims that
those exact directories were recovered from BDS. A matching type name does not
prove matching members or responsibilities. Unmatched classes retain their names.

## Domains

- `World/Actor`: actors, mobs, AI, movement; player classes are under `Player`.
- `World/Level`: world state, `Chunk`, `Storage`, `Dimension`, `Biome`, `LevelGen`,
  block classes and block actors. Concrete blocks live directly under `Block`.
- `World/Item`: item classes and components. Concrete items live directly here.
- `World/Inventory` and `World/Containers/Managers/Models`: inventories and container
  models respectively.
- `Network`: network data and `ServerNetworkHandler`; Falcon's delegated handlers
  remain under `Handler`.
- `Server`: lifecycle and configuration, with commands under `Commands`.
- `Resources` and `Scripting`: resource packs and scripting.
- `Core`: reusable support; task queues use `Core/Threading`.
- `Falcon`: the existing public embedding API, plus Falcon-specific implementation
  under `Embedded` and `Plugin`.

## Validation and plugin migration

Run `python3 Falcon.Server/tools/verify_structure.py` from the repository root to
check the inventory, directory destinations and obsolete owned include paths.
During this refactor, `--verify-content` additionally proves that every inventoried
file's original contents are preserved except for `#include` operands. That mode
compares against the historical migration snapshot; it is not intended to reject
future gameplay edits. Update the inventory when adding or removing source files.

Build with `cmake --build build` and run
`ctest --test-dir build/Falcon.Server --output-on-failure`. CMake source exclusions
and generated header inputs use the new locations. No changes to fetched dependency
headers are needed.

Internal C++ plugins should use the inventory to adopt the new include paths and
rebuild with the matching SDK. Existing includes continue to work through forwarding
headers generated under `build/Falcon.Server/legacy-include/`, also packaged in the
SDK. These are build artifacts; the old source directories are not restored. For example,
`Actor/ServerPlayer.h` becomes `World/Actor/Player/ServerPlayer.h` and
`Network/Handler/ServerNetworkHandler.h` becomes `Network/ServerNetworkHandler.h`.
The standalone plugin C API belongs to the separate PluginAPI repository.
