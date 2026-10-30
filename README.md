<p align="center"><img src="docs/assets/shan-hai.png" alt="mountains&amp;seas — 山&amp;海" width="720"></p>

# mountains&seas · 山&海

English · [简体中文](README.zh-CN.md)

[Download 0.3.7](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.7) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

mountains&seas is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.7.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.7.zip` |

Windows x64 and an AVX2-capable CPU. Select the package matching your game version.

## Install

1. Download the ZIP matching your game version from the [0.3.7 release](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.7), using the compatibility table above.
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The launcher entry is **mountains&seas**, by **troveminecraft**. For upgrades from the old entry, follow the migration steps below.

## Version 0.3.7

Chunk scheduling now retains waiting time across generation stages, bounds the priority bonus of old requests, and promotes eligible structure, decoration and lighting work so nearby terrain can finish before the full distant view is generated. The nearest dependency window includes both edges at fractional and negative player coordinates. Native dependency checks remain authoritative.

New worlds use kernel **2.213** and a continuous coastal altitude fade. This removes an artificial step where the old coastal lift was cut off before its fade ended. Existing worlds keep their saved recipe and already generated blocks; the coastal correction is enabled in new worlds. Sea level remains **Y100**, with a building range of **Y-64..511**.

Client mesh timing now accepts the renderer’s separate client world. The bundled seed runtime no longer imports test modules or ships Python/NumPy test extensions and bytecode caches. Loading stalls under dense terrain, large view distances or memory pressure may still occur; offline dependency simulations do not establish complete game loading performance.


## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
