# mountains&seas · 山&海

Fixes narrow spikes along curved cliff rims by joining the two banks directly, preserving cliff height, contour and end transitions. Existing worlds retain their saved cliff recipe.

New 26.51 worlds use fewer high cliffs, with occasional small trees rooted in the exposed faces. Shallow differential-weathering grooves vary mountain slopes while retaining smooth rock domes. Dark and pale forests have irregularly spaced trunks and asymmetric hollows open to natural ground; roots use bark-covered wood, and creaking hearts have continuous ground support. Large villages retain their building count and use compact irregular plots with connected roads. Biome locating first surveys lightweight candidates, verifies results against the saved terrain recipe, and reuses a bounded world-local query cache separate from chunk generation.

English · [简体中文](README.zh-CN.md)

Windows LL **0.4.0**, BDS terrain **1.0.63**, structures **1.0.78**, kernel **2.221**. Android is archived.

Dark and pale forests now share connected canopies supported by 40–50-block trees with trunk radii of 5–7 blocks. Arched hollows can contain a jack-o’-lantern (66% per hollow); pale hollows can also contain a naturally generated creaking heart (11%). Glow lichen grows on supported root faces. Cherry groves form coherent 160–480-block stands, each with a central tree about 70 blocks high and a trunk radius of 10–20 blocks. Gentle river reaches can split into surveyed side channels and rejoin downstream. Villages target six times the previous building count on compact, irregular plots with connected roads, retaining native building templates. Initial resident and animal counts remain at the original scale to avoid multiplying AI work. Unsuitable terrain and layouts exceeding the bounded village footprint are rejected.

0.4.0 is in development; its previous release has been withdrawn.

Fixes a world-loading exit caused by duplicate persistent-leaf palette entries. Saved terrain recipes remain compatible. In mountains&seas worlds, use names such as `/locate biome peakarma:roofed_forest` or `/locate biome peakarma:super_glacier`. Suggestions and results use the mod's biome catalog. Windows command suggestions include biome descriptions; descriptions and query messages use English by default and Chinese for a Chinese game language. Snow-covered variants are searched separately.

Adds dark forests, pale gardens, cherry groves and mangrove swamps to new worlds on both supported game versions. Minecraft 1.26.51.01 also gains dappled forests with poplar foliage, fallen logs and shelf mushrooms. Forests follow the existing climate and terrain.

Pale trees include native natural creaking hearts; mangrove roots preserve water in the secondary block layer. Woodland mansions now require an actual dark forest. `/karma` adds forest entries 141–144, plus 145 on 26.51; existing numbers and optional teleport modes remain unchanged.

New mangrove recipes use denser stands in warm inland basins, with submerged roots and a continuous span of at least 160 blocks. `/locate biome` uses the world's saved mountains&seas recipe and runs its search in the background; ordinary worlds retain vanilla behavior.

New 26.51 worlds use recipe **2.221** and a fixed game capability profile. Existing worlds retain their saved recipe and terrain, including 2.217. A world using 26.51-only forest content cannot open on 26.20. Sea level remains Y100 and the building range remains Y-64..511. The update also fixes world-to-world biome table reuse and seed preparation under non-ASCII paths.

Dense terrain, large view distances and native structure processing can still cause loading delays. Nighttime creaking behavior and new forest save/reload behavior need further compatibility coverage across the supported game builds.

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.4.0.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.4.0.zip` |

BDS requires **1.26.20.5 / LeviLamina 26.20.7**. No 26.51 BDS package is included. Save and stop before updating plugins; retain existing worlds and height packs.

Original code uses [MIT](LICENSE); third-party components retain their [licenses](THIRD_PARTY_NOTICES.md).
