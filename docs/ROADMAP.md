# 0.4.x maintenance priorities

Version 0.4.0 closes the forest and landscape feature scope. The following work continues in 0.4.x; these entries do not claim fixes already present in 0.4.0.

- Diagnose terrain sections that remain invisible beneath visible foliage until approached. Distinguish generation, native structure waits, mesh building and GPU publication before selecting a fix.
- Improve loading in unexplored regions and during native structure processing without changing frozen terrain recipes or relying only on chunk re-requests.
- Improve `/locate biome` latency and coverage while keeping queries independent of chunk generation. Keep incomplete searches distinct from absence within the searched range.
- Investigate lossless reductions to saved-world disk usage beyond the existing derived-cache compression and budget. Preserve player blocks, entities and world history.
- Continue targeted terrain and vegetation corrections from reproducible seeds and coordinates, including tree silhouettes and river/lake joins.

Windows 26.51 maintenance does not automatically upgrade BDS or the retained Windows 26.20 package. Exact game and loader versions, kernel revisions and package source commits remain explicit in each release.
