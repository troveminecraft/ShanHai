<p align="center"><img src="docs/assets/shan-hai.png" alt="mountains&amp;seas — 山&amp;海" width="720"></p>

# mountains&seas · 山&海

English · [简体中文](README.zh-CN.md)

[Download 0.3.3](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.3) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

mountains&seas is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

- Minecraft for Windows **1.26.51.01**.
- LeviLamina **26.51.9**, client edition.
- Windows x64 and an AVX2-capable CPU.

## Install

1. Download [ShanHai-Windows-0.3.3.zip](https://github.com/troveminecraft/ShanHai/releases/download/v0.3.3/ShanHai-Windows-0.3.3.zip).
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The launcher entry is **mountains&seas**, by **troveminecraft**. For upgrades from the old entry, follow the migration steps below.

## Version 0.3.3

Fixes a world-entry failure on Minecraft **1.26.51.01** caused by reading the saved NBT seed name instead of the runtime seed property. Terrain and structures now read the native 64-bit seed consistently, and world initialization checks it against the creation settings. Optional session diagnostics no longer interrupt world cleanup.

LeviLauncher now displays **mountains&seas**, by **troveminecraft**. The install directory is `mods/mountains&seas`; the DLL filename and world markers remain compatible. When upgrading from 0.3.2 or earlier, fully close Minecraft, back up and move the old `karma-worldgen-client` folder outside `mods`, then import this ZIP. Copy `client-config.json` and, optionally, `seed-cache` from the backup into the new folder. Do not enable both entries.

Requires LeviLamina **26.51.9**, Windows x64 and AVX2. Kernel **2.211**, terrain recipes, sea level **Y100** and building range **Y-64..511** are unchanged. First entry and exploration can still show loading stalls or visible chunk gaps. Vanilla worlds are not automatically converted.

Please report the version, world seed, coordinates, screenshots and reproduction steps when you encounter an issue.

Back up worlds before updating. Already generated chunks are not rewritten. Supported recipes through 0.1.13 and from 0.1.16 retain their original terrain. Experimental 0.1.14/0.1.15 worlds using kernel 2.204 must use their retained matching version; this release does not silently migrate them. Keep the mod enabled when continuing a mod-generated world.

This download is for the listed Windows client environment. This 26.51 update applies to Windows only. Android is archived, and Java Edition is not supported.

## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
