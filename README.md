<p align="center"><img src="docs/assets/shan-hai.png" alt="ShanHai — Mountains & Seas" width="720"></p>

# ShanHai · 山&海

English · [简体中文](README.zh-CN.md)

[Download 0.2.0](https://github.com/troveminecraft/ShanHai/releases/tag/v0.2.0) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

ShanHai is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

- Minecraft for Windows **1.26.20.04**.
- LeviLamina **26.20.7**, client edition.
- Windows x64 and an AVX2-capable CPU.

## Install

1. Download [ShanHai-Windows-0.2.0.zip](https://github.com/troveminecraft/ShanHai/releases/download/v0.2.0/ShanHai-Windows-0.2.0.zip).
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The internal mod ID remains `karma-worldgen-client` for configuration and save compatibility.

## Version 0.2.0

New worlds use sea level **Y100**, raised land and inland basins with **Y60–100** floors. Explore low, middle and high altitude variants, broad plateaus and three sizes of rifts. Altitude and snow transitions are continuous; the build range remains **Y-64 through Y511**. Keep the world's bundled height behavior pack enabled.

Snowy landscapes retain the parent biome's ground and vegetation. A broad snow belt surrounds super glaciers; ice spires have wider roots and less extreme lean. Coastal volcanic rock can form obsidian where it meets water. Loading recovery now prioritizes the dependencies of visible missing chunks and retries stalled requests within a fixed limit. See [elevation and climate](docs/ELEVATION.md) for the generation rules.

**This is a test release awaiting gameplay acceptance.** Offline generation tests and successful builds do not establish in-game correctness. Please report the version, world seed, coordinates, screenshots and reproduction steps when you encounter an issue.

Back up worlds before updating. Already generated chunks are not rewritten. Supported recipes through 0.1.13 and from 0.1.16 retain their original terrain. Experimental 0.1.14/0.1.15 worlds using kernel 2.204 must use their retained matching version; this release does not silently migrate them. Keep the mod enabled when continuing a mod-generated world.

This download is for the listed Windows client environment. BDS has a separate package; Android is archived, and Java Edition is not supported.

## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
