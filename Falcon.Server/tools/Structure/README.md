# Falcon server source layout

The `include/` and `src/` trees are grouped by domain, using PascalCase directories to satisfy Falcon's
repository rules. The separate `Core/` module and the public `Falcon/EmbeddedServer.h` embedding API keep
their existing names.

This is a file organization refactor. C++ class names, inheritance, responsibilities, serialization,
protocol and gameplay are unchanged.

## Inventory

The machine-readable [inventory](inventory.json) records every server source and header file moved by
the refactor: its old location, its new location, the hash of its original contents and the classes it
declares. Its `baseline` field identifies the Falcon commit before the move.

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
