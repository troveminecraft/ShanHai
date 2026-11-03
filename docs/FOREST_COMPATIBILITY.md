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
- Mangrove swamp retains native warm-variant and frog tags. New forest revision 2 selects warm, humid inland basins with existing shallow water and a continuous core spanning at least 160 blocks. Trees use a denser stand, with roots embedded in submerged mud and water in the secondary block layer. Dry land, coastal mudflats and rifts cannot substitute for a basin. Revision 1 worlds retain their original coastal recipe.
- Dappled forest uses the native cold-animal tags and red/orange/yellow poplar foliage. Shelf mushroom size uses `growth` (0/1), with `minecraft:cardinal_direction` controlling attachment. The 26.20 profile excludes its selection and palette entries before registry resolution.

The 26.51 biome assets add `has_structure_abandoned_camp` to pale gardens and cherry groves and include it in dappled forests. This expansion does not add an abandoned-camp generator; the custom world generator continues to use its existing structure allowlist.

## Compatibility and verification

Existing numeric block and biome entries are frozen; new entries append. Old recipes keep their exact tables and generation behavior. New recipes identify the target capability profile, and both profile and recipe contents participate in cache identity. A 26.51-only world must not silently substitute missing blocks when opened on 26.20.

Recipe profiles isolate block registries and seed caches. The compatibility table describes available native content, not identical package revisions: the current 26.51 package uses kernel 2.224, while the retained Windows 26.20 package uses 2.219. This 26.51 revision does not update BDS.

The table above lists native biome asset identifiers. The mod command catalog instead uses `peakarma:` names and localized descriptions. In an active mountains&seas Overworld, it searches the current world's frozen recipe on a single cancellable background worker, without requesting chunks or borrowing a generation worker. Ordinary worlds and other dimensions retain the native command. Region-owned targets use direct candidates up to 16,384 blocks away; other targets use an 8,192-block radius, with a 45-second work deadline; a timeout is reported as incomplete, not as proof that the biome does not exist. Returned coordinates are checked against the actual generated biome; they are nearby matches, not an exhaustive nearest-block guarantee.


## Canopy recipes

New forest revision 3 uses `bedrock-26.20-forests-v2` or `bedrock-26.51-forests-v2`. Previous v1 profiles and saved revisions remain unchanged. Both v2 palettes include oriented jack-o’-lanterns, supported glow lichen and persistent canopy leaves. The 26.20 profile maps the 26.51-only poplar and shelf-mushroom slots to air without resolving unsupported native blocks.

Dark and pale forests now share connected canopies supported by 40–50-block trees with trunk radii of 5–7 blocks. Arched hollows can contain a jack-o’-lantern (66% per hollow); pale hollows can also contain a naturally generated creaking heart (11%). Glow lichen grows on supported root faces. Cherry groves form coherent 160–480-block stands, each with a central tree about 70 blocks high and a trunk radius of 10–20 blocks. Gentle river reaches can split into surveyed side channels and rejoin downstream. Villages target six times the previous building count on compact irregular plots with connected roads, retaining native building templates. Initial resident and animal counts remain at the original scale to avoid multiplying AI work. Unsuitable terrain and layouts exceeding the bounded village footprint are rejected.

Large trees retain compact descriptors and write only the current chunk. River branches reuse existing owner plans; village streets use the existing candidate grid with deterministic separation for larger footprints; native chunk dependency requests are unchanged. Per-section missing terrain remains a separate known rendering/loading issue. Builder measurements do not include GPU publication.

## Current 26.51 generation revision

Kernel 2.223 widens basin landforms while retaining interior relief. Cherry revision 2 uses a compact branch skeleton with separate foliage clusters instead of a continuous elliptical roof; the largest groves favour Y200–300. Each chunk writes its own clipped part of a deterministic tree. Saved recipes retain earlier tree and basin shapes. Native structure reservations exclude a whole giant tree when its roots overlap the occupied footprint.

Kernel 2.224 selects cherry revision 3 for new worlds. Curved leaders divide into secondary branches and fine twigs; small asymmetric flower sprays follow those twigs instead of forming rounded caps on heavy spokes. The roughly 70-block height and 10–20-block base radius remain. Each worker retains at most 16 compact skeletons and clips writes to the current chunk. No new chunk requests, native structures or block palette entries are introduced. Existing cherry revisions remain available for frozen recipes.
