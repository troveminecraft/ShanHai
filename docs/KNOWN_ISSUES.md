# Known issues — 0.4.0

- Some terrain sections remain invisible while foliage above them is visible, then recover when approached. Native structure lock stalls have been observed, but they do not by themselves explain persistent missing mesh sections. Per-height builder diagnostics now distinguish these cases; GPU publication is not measured.
- Large view distances, cold regional planning, dense forests and native structure processing can still delay loading. Kernel benchmarks exclude the complete game/rendering pipeline.
- Structures, roads and vegetation can have imperfect joins. The ruined-portal vegetation footprint fix prevents excessive clearance in newly processed terrain; it does not restore blocks previously removed from a saved world.
- `/locate biome` uses bounded searches: 16,384 blocks for region-owned targets, 8,192 for general targets, and a 45-second deadline. An incomplete search is not proof of absence. Results are verified nearby matches, not exhaustive nearest-block results.
- Updating retains the world's frozen terrain recipe and existing blocks. There is no automatic old-world terrain migration or guaranteed seamless generation after uninstalling the mod.
- Current 26.51 packages require Minecraft 1.26.51.01 / LL 26.51.9. Retained Windows 26.20 packages require Minecraft 1.26.20.04 / LL 26.20.7 and contain an earlier 0.4.0 revision. BDS packages target 1.26.20.5 / LL 26.20.7 separately. Android is archived.
- Disposable derived caches are bounded separately from the saved world. New exploration still increases LevelDB world size; player blocks and unexplored-region boundaries are not deleted to reduce disk use.

For reproducible reports, include the exact package/kernel identity, game and loader build, seed, coordinates, actions and a screenshot. The local diagnostic collector does not upload data.

See the [0.4.x maintenance roadmap](ROADMAP.md).
