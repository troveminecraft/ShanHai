# mountains&seas · 山&海

A terrain and structure generation mod for Minecraft Bedrock on Windows. Author: **peakarma**. Version **0.4.0 is in development**; the previous 0.4.0 release has been withdrawn.

English · [简体中文](README.zh-CN.md)

Sea level is **Y100**, with building space at **Y-64..511**. Mountains, broad inland basins, plateaus, rivers, lakes, waterfalls, volcanoes and super-glaciers share a seed-based terrain recipe. Basins retain internal hills and valleys rather than defining a single ecological biome.

The current Windows 26.51 revision uses kernel **2.224**. Giant cherry trees have curved, tapering branches, multiple levels of forks and small flower sprays distributed along the outer twigs. River beds continue into lakes without an abrupt sand wall at the shoreline. Wider basins, taller small cherries, larger boulders and fewer snowy-plains fallen logs are retained. Cherry groves favour Y200–300 for their largest stands. Ruined portals use their occupied footprint for vegetation clearance; intersecting giant-tree roots exclude the whole tree.

Dark and pale forests have connected canopies, 40–50-block trees with 5–7-block trunk radii, irregular spacing and natural-ground hollows. A hollow has a 66% chance of a jack-o’-lantern; pale hollows have an 11% chance of a natural creaking heart. Roots use bark-covered wood with supported glow lichen. Warm inland basins support dense mangroves rooted in shallow water. Windows 26.51 also supports native dappled-forest foliage and shelf mushrooms.

Villages target six times the earlier building count on compact, irregular plots with connected roads. Native building templates and the original initial resident/animal scale are retained. Terrain and bounded-footprint checks can reject unsuitable sites. Super-glacier cliffs carry a thin ice skin; grass and ferns can share a native block cell with thin snow.

## Biome search

In a mountains&seas world, `/locate biome peakarma:roofed_forest` searches the saved mod recipe. Suggestions and results include descriptions, in English by default and Chinese for a Chinese game language. Ordinary worlds retain the native command.

Glaciers, volcanoes, cherry groves, mangroves and inland basins use regional candidates up to 16,384 blocks away. Other searches use an 8,192-block radius. Results are verified against the full recipe; queries run on a separate cancellable background worker, without requesting world chunks. Results are nearby matches, not an exhaustive nearest-block guarantee. A 45-second work limit can produce an incomplete result.

## Compatibility

| Minecraft for Windows | LeviLamina client | Retained package | Kernel revision |
|---|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.4.0.zip` | 2.224 |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.4.0.zip` | 2.219 |

These packages are different development revisions. The latest 26.51 changes are not included in the retained 26.20 package. BDS supports **1.26.20.5 / LeviLamina 26.20.7** through its separate packages; this 26.51 revision does not update BDS. Android is archived. Other game/loader builds are unsupported.

Save and fully exit Minecraft before importing the matching ZIP into LeviLauncher. Enable **mountains&seas** and create a normal world. ZIP packages include their Python and Node.js runtime. Keep the previous mod folder outside `mods` as a rollback copy; do not enable duplicate copies.

Existing worlds keep their saved terrain recipe and generated blocks. New generation features require a new world recipe; updating the DLL does not redraw an old world. Worlds containing 26.51-only forest content cannot open on 26.20. Removing the mod does not guarantee compatible future terrain.

## Storage and known limits

Derived terrain caches use lossless compression with a background-managed 512 MiB budget per world recipe. They are separate from saved blocks, structures and player data. Loading diagnostics retain three recognized sessions. Exploring new regions still grows the saved world.

Some terrain sections may remain invisible while higher foliage sections are visible until the player approaches. This remains under investigation; generation timings alone do not establish a rendering fix. Dense terrain, distant exploration and native structure processing can still cause delays. See [known issues](docs/KNOWN_ISSUES.md), [loading diagnostics](docs/LOADING_DIAGNOSTICS.md), [forest compatibility](docs/FOREST_COMPATIBILITY.md) and [building from source](docs/DEVELOPMENT.md).

Original code uses [MIT](LICENSE); third-party components retain their [licenses](THIRD_PARTY_NOTICES.md).
