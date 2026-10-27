<p align="center"><img src="docs/assets/shan-hai.png" alt="ShanHai — Mountains & Seas" width="720"></p>

# ShanHai · 山&海

English · [简体中文](README.zh-CN.md)

[Download 0.2.2](https://github.com/troveminecraft/ShanHai/releases/tag/v0.2.2) · [Changelog](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Report an issue](https://github.com/troveminecraft/ShanHai/issues)

ShanHai is an Overworld terrain generation mod for Minecraft Bedrock on Windows, powered by LeviLamina. Follow rivers through mountain ranges, rolling highlands, forests, deserts and coastlines; explore caves, glaciers and volcanic terrain. Vanilla structures are placed according to the surrounding landscape.

## Requirements

- Minecraft for Windows **1.26.20.04**.
- LeviLamina **26.20.7**, client edition.
- Windows x64 and an AVX2-capable CPU.

## Install

1. Download [ShanHai-Windows-0.2.2.zip](https://github.com/troveminecraft/ShanHai/releases/download/v0.2.2/ShanHai-Windows-0.2.2.zip).
2. Save and fully close Minecraft. Import the ZIP into the matching instance through LeviLauncher’s mod manager.
3. Enable the mod and create a new **normal** world with your chosen seed. Do not select a flat world.

Use the release ZIP, rather than GitHub’s automatically generated source archive. Python and Node.js runtime dependencies are included; copying only the DLL is insufficient. The internal mod ID remains `karma-worldgen-client` for configuration and save compatibility.

## Version 0.2.2

This update addresses a Windows local-world loading failure: the height pack is registered with the new world's native pack source and selected in the in-memory startup settings. A missing or conflicting height resource is rejected before terrain generation starts. Failed native initialization retains its generation session until the engine closes the world.

Terrain remains the **0.2.1 recipe**: sea level **Y100**, basin floors **Y60–100**, and build range **Y-64 through Y511**. Existing saves and generation rules are unchanged. BDS remains terrain **1.0.48** / structures **1.0.63**; Android is archived. Keep the world's bundled height behavior pack enabled.

**This is a test release awaiting gameplay acceptance.** Offline generation tests and successful builds do not establish in-game correctness. Please report the version, world seed, coordinates, screenshots and reproduction steps when you encounter an issue.

Back up worlds before updating. Already generated chunks are not rewritten. Supported recipes through 0.1.13 and from 0.1.16 retain their original terrain. Experimental 0.1.14/0.1.15 worlds using kernel 2.204 must use their retained matching version; this release does not silently migrate them. Keep the mod enabled when continuing a mod-generated world.

This download is for the listed Windows client environment. BDS has a separate package; Android is archived, and Java Edition is not supported.

## Source and license

This repository contains the shared terrain and structure rules, Windows adapter and build tools. See [BUILDING.md](docs/BUILDING.md), [kernel boundaries](docs/KERNEL_BOUNDARY.md) and [runtime documentation](docs/RUNTIME.md).

Original project code is licensed under [MIT](LICENSE). Third-party components and derived resources retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Minecraft itself and vanilla structure assets are not redistributed; templates are read from the local game installation.

Windows startup now recognizes the measured LeviLauncher/PreLoader layouts without relaxing the pinned game-code checks. Worlds created while the mod failed to load are vanilla worlds and are not automatically converted; create a new normal world with the mod enabled.
