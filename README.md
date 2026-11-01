# mountains&seas · 山&海 — BDS upstream

English · [简体中文](README.zh-CN.md)

Windows LL **0.4.0**, BDS terrain **1.0.63**, structures **1.0.78**, kernel **2.219**. Android is archived.

0.4.0 is in development; its previous release has been withdrawn.

Adds dark forests, pale gardens, cherry groves and mangrove swamps to new worlds on both supported game versions. Minecraft 1.26.51.01 also gains dappled forests with poplar foliage, fallen logs and shelf mushrooms. Forests follow the existing climate and terrain; occasional large trees remain local features.

Pale trees include native natural creaking hearts; mangrove roots preserve water in the secondary block layer. Woodland mansions now require an actual dark forest. `/karma` adds forest entries 141–144, plus 145 on 26.51; existing numbers and optional teleport modes remain unchanged.

New mangrove recipes use denser stands in warm inland basins, with submerged roots and a continuous span of at least 160 blocks. `/locate biome` uses the world's saved mountains&seas recipe and runs its search in the background; ordinary worlds retain vanilla behavior.

New worlds use recipe **2.219** and a fixed game capability profile. Existing worlds retain their saved recipe and terrain, including 2.217. A world using 26.51-only forest content cannot open on 26.20. Sea level remains Y100 and the building range remains Y-64..511. The update also fixes world-to-world biome table reuse and seed preparation under non-ASCII paths.

Dense terrain, large view distances and native structure processing can still cause loading delays. Nighttime creaking behavior and new forest save/reload behavior need further compatibility coverage across the supported game builds.

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.4.0.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.4.0.zip` |

BDS requires **1.26.20.5 / LeviLamina 26.20.7**. No 26.51 BDS package is included. Save and stop before updating plugins; retain existing worlds and height packs.

Original code uses [MIT](LICENSE); third-party components retain their [licenses](THIRD_PARTY_NOTICES.md).
