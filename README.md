<p align="center"><img src="docs/assets/shan-hai.png" alt="mountains&amp;seas — 山&amp;海" width="720"></p>

# mountains&seas · 山&海

English · [简体中文](README.zh-CN.md)

[Download 0.3.9](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.9) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

mountains&seas is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.9.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.9.zip` |

Windows x64 and an AVX2-capable CPU. Select the package matching your game version.

## Install

1. Download the ZIP matching your game version from the [0.3.9 release](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.9), using the compatibility table above.
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The launcher entry is **mountains&seas**, by **troveminecraft**. For upgrades from the old entry, follow the migration steps below.

## Version 0.3.9

New worlds preserve river banks against the actual neighbouring water surface and retain closed lake shorelines during river erosion. Regional river planning shares the lakes touched by its routes, uses surveyed lake endpoints and rejects crossings that would cut a lake at a different water level.

Mountain cliff cores can drop **60–100 blocks within two horizontal blocks**. Rivers can cross suitable **30–100-block ledges** as connected waterfalls. Cliff ends and surrounding slopes blend back into the terrain. Sea level remains **Y100**, with a building range of **Y-64..511**.

Kernel **2.215** preserves saved world recipes. These generation changes apply to new worlds; existing blocks are not rewritten. Bounded terrain caches and lazy lake planning limit repeat sampling. Dense terrain, large view distances or memory pressure can still cause loading stalls; offline checks do not establish full game loading performance.


## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
