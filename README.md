<p align="center"><img src="docs/assets/shan-hai.png" alt="mountains&amp;seas — 山&amp;海" width="720"></p>

# mountains&seas · 山&海

0.4.0 is in development; its previous release has been withdrawn. New mangrove recipes use dense shallow-water stands in warm inland basins, spanning at least 160 blocks. `/locate biome` searches the saved mountains&seas recipe in the background; ordinary worlds retain vanilla behavior.

English · [简体中文](README.zh-CN.md)

[Download 0.3.12](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.12) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

mountains&seas is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.4.0.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.4.0.zip` |

Windows x64 and an AVX2-capable CPU. Select the package matching your game version.

## Install

1. Download the ZIP matching your game version from the [0.4.0 release](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.12), using the compatibility table above.
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The launcher entry is **mountains&seas**, by **troveminecraft**. For upgrades from the old entry, follow the migration steps below.

## Version 0.4.0

Adds dark forests, pale gardens, cherry groves and mangrove swamps to new worlds on both supported game versions. Minecraft 1.26.51.01 also gains dappled forests with poplar foliage, fallen logs and shelf mushrooms. Forests follow the existing climate and terrain; occasional large trees remain local features.

Pale trees include native natural creaking hearts; mangrove roots preserve water in the secondary block layer. Woodland mansions now require an actual dark forest. `/karma` adds forest entries 141–144, plus 145 on 26.51; existing numbers and optional teleport modes remain unchanged.

New worlds use recipe **2.217** and a fixed game capability profile. Existing worlds retain their saved recipe and terrain, including 2.216. A world using 26.51-only forest content cannot open on 26.20. Sea level remains Y100 and the building range remains Y-64..511. The update also fixes world-to-world biome table reuse and seed preparation under non-ASCII paths.

Dense terrain, large view distances and native structure processing can still cause loading delays. Nighttime creaking behavior and new forest save/reload behavior need further compatibility coverage across the supported game builds.


## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
