# Forest compatibility

The forest ecology expansion uses native Bedrock biome identities. Selection, terrain adaptation and vegetation are shared by the maintained adapters; game capabilities are an immutable part of each new world recipe.

| Forest | Bedrock identifier | Windows 26.20 / BDS 26.20 | Windows 26.51 |
|---|---|---|---|
| Dark forest | `minecraft:roofed_forest` | Supported | Supported |
| Pale garden | `minecraft:pale_garden` | Supported | Supported |
| Cherry grove | `minecraft:cherry_grove` | Supported | Supported |
| Mangrove swamp | `minecraft:mangrove_swamp` | Supported | Supported |
| Dappled forest | `minecraft:dappled_forest` | Unavailable | Supported |

Identifiers and feature block states were checked against the installed 1.26.20.04 and 1.26.51.01 behavior packs, including the indexed JSON entries in `__brarchive`, and the 26.20 BDS behavior packs. Language strings alone are not capability validation. Runtime registry resolution must also verify every requested block permutation and biome.

Pale gardens were introduced in [Bedrock 1.21.50](https://feedback.minecraft.net/hc/en-us/articles/32344904160397-Minecraft-Bedrock-Edition-1-21-50-The-Garden-Awakens). Dappled forests were experimental in [26.40](https://www.minecraft.net/en-us/article/minecraft-bedrock-edition-26-40) and became a normal feature in [26.50](https://feedback.minecraft.net/hc/en-us/articles/48826825649933-Minecraft-Bedrock-Edition-26-50-Changelog-Wilderness-Bound).

## Native behavior boundaries

- Dark forest retains the native `roofed` identity, ordinary forest animals and dark oak/mushroom materials. Woodland mansions use the existing structure adapter only where a real dark forest and suitable footprint exist.
- Pale garden has no `animal` tag. Native pale foliage, hanging moss and hearts supply its atmosphere. A generated heart must be natural, surrounded by pale logs with matching pillar orientation, and own a native block actor. Volume writes alone are insufficient evidence of functioning heart behavior.
- Cherry grove keeps native grass/foliage tints and `bee_habitat`. Petal direction and growth must survive save/load; plants need actual support.
- Mangrove swamp retains native warm-variant and frog tags. Root water belongs in the secondary block layer, independently of the primary root block. Warm coastal placement does not override the existing dry-biome exclusion rules.
- Dappled forest uses the native cold-animal tags and red/orange/yellow poplar foliage. Shelf mushroom size uses `growth` (0/1), with `minecraft:cardinal_direction` controlling attachment. The 26.20 profile excludes its selection and palette entries before registry resolution.

The 26.51 biome assets add `has_structure_abandoned_camp` to pale gardens and cherry groves and include it in dappled forests. This expansion does not add an abandoned-camp generator; the custom world generator continues to use its existing structure allowlist.

## Compatibility and verification

Existing numeric block and biome entries are frozen; new entries append. Old recipes keep their exact tables and generation behavior. New recipes identify the target capability profile, and both profile and recipe contents participate in cache identity. A 26.51-only world must not silently substitute missing blocks when opened on 26.20.

Development keeps the installed 0.3.12 baseline untouched. Regression compares old-recipe bytes and new-recipe determinism separately. Cold/warm exploration includes calculator queues and longest completion gaps, rather than only prewarmed average throughput. Native entity behavior, persistence and visual appearance require their own checks; compilation is not gameplay acceptance.
