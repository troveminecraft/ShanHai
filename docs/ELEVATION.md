# Elevation and climate in 0.2.1

New generation is opt-in through immutable `elevationRevision=2` recipes (kernel 2.209). Existing recipes retain their previous heights, materials and climate. The sea is Y100; the build range stays -64 through 511. Marine variants describe depth and cold-water conditions, not floating high-altitude oceans.

| Land type | Core altitude | Transition |
| --- | --- | --- |
| Basin floor | 60–100 | Closed inland bowl, with a rim above the sea and local water budget |
| Low altitude | Below roughly 155 | Middle-altitude weight increases continuously through 135–175 |
| Middle altitude | Roughly 155–280 | High-altitude weight increases through 255–305 |
| High altitude | Above roughly 280 | Less tree cover, increasingly cold; original peak ceiling retained |
| Plateau | Above 150 | Broad, gently rolling surfaces and gradual edges |

These are game-scale choices, not universal real-world elevation definitions. Biome identity and terrain elevation are separate. Parent vegetation and materials are retained under a snow overlay. The appended climate-carrier palette uses cold vanilla biomes for stable snow/weather; it does not register 29 new engine biome definitions. Parent IDs remain available in generation diagnostics.

Basins are surveyed over their footprint and a surrounding ring before their depression is applied. They cannot use their own lowered floor as proof of an ocean connection. The hydrology planner distinguishes marine sinks from closed inland water below sea level. The three canyon sizes have nominal half-widths 3.5/5.5/9 and maximum cuts 38/66/104 blocks. Width, depth, sides and tapered ends vary along curved axes. Inland cuts can extend below sea level; they do not create an automatic ocean connection. Basin floors are wider, with angular and spatial contour variation, and are excluded from sea-level beach classification.

Glacier snow distance is measured from the region boundary: full cover to 512 blocks, a continuous retreat through 1024 blocks. Geometry and surface-material patches are independent. Snow patches retain sand, terracotta and mycelium beneath the surface; volcanic heat suppresses snow in the centre.

## Geographic references

- [NPS: Death Valley geology](https://www.nps.gov/deva/learn/nature/geology.htm): below-sea-level inland basins, surrounding relief, erosion and sedimentation.
- [NPS: Life in the Zone](https://www.nps.gov/common/uploads/teachers/lessonplans/Life-in-the-Zone%20Rangers%20in%20the%20Classroom.pdf): elevational zones overlap, with aspect and rain shadow affecting ecology.
- [USGS: Geology of grabens](https://pubs.usgs.gov/publication/70227388): fault-bounded depressions and asymmetry.
- [NPS: Pillow basalts](https://www.nps.gov/articles/000/pillow-basalts.htm): lava-water contact and chilled glassy surfaces. Obsidian patches here follow Minecraft's lava/water convention, not a claim that all basaltic lava turns into obsidian.

## Loading changes

Recovery scans the requested view up to 64 chunks. It keeps at most two disjoint 5×5 core patches per player, each with eight dependency rings beyond its core. Patches remain until the entire core is ready, the player moves away, or 45 seconds pass. Distant repair work is excluded from the near-player backlog count. A gap receives no more than three attempts while it stays visible, with 30/60-second retry backoff. Recovery never marks incomplete native chunks loaded or repeatedly resends the entire view. Diagnostics report the actual patch count and scan interval.

Offline geometry, deterministic output and startup checks are separate from gameplay acceptance. Live loading latency must be measured after deployment under comparable exploration conditions.
