## 0.3.2 — Windows world-loading crash

Fixes a crash while loading a new world on Minecraft **1.26.51.01**. The native terrain-generator constructor passes a biome number directly, but the previous hook interpreted it as a pointer. The adapter now preserves the native calling convention and validates the corresponding game instructions. The 0.3.1 activation-order fix is retained.

The English product name is **mountains&seas**. ZIP metadata now includes **troveminecraft** as the author for LeviLauncher. Existing internal IDs, save paths and package filenames remain compatible.

Requires LeviLamina **26.51.9**, Windows x64 and AVX2. Terrain recipes, kernel **2.211**, sea level **Y100** and building range **Y-64..511** are unchanged. Known issue: first entry and exploration may still show loading stalls or visible chunk gaps. Worlds created while the generator was disabled remain vanilla worlds.

This 26.51 update applies to Windows only.

## 0.3.1 — Windows generator activation

World creation now activates before cloud rendering patches are installed. This fixes 0.3.0 reporting the DLL as loaded but then rejecting its own patched executable, disabling the terrain hooks and creating vanilla worlds. Strict executable compatibility checks remain enabled. Startup diagnostics now distinguish a loaded DLL from a fully enabled generator.

Minecraft **1.26.51.01** and LeviLamina **26.51.9** are required. Terrain recipes, kernel **2.211**, sea level **Y100** and building range **Y-64..511** are unchanged. Worlds created while activation failed remain vanilla worlds; create a new normal world after updating.

Known issue: first entry and exploration can still show loading stalls or visible chunk gaps. This update addresses generator activation, not those separate performance problems.

Windows **0.3.1**. This 26.51 update applies to Windows only.

## 0.3.0 — Bedrock 26.51 compatibility

Windows now targets Minecraft **1.26.51.01** with LeviLamina **26.51.9**. World creation, height-pack discovery, chunk views, save-state tracking and native structure placement use the new game interfaces. The shared source retains a separate 26.20 build path; DLLs for different game builds are not interchangeable.

The Y500 cloud adjustment uses the two native cloud-height call sites shared by original graphics and Vibrant Visuals. Executable layout and instruction checks reject unsupported builds. Native chunk task timing follows each task’s owning chunk, and client mesh-build timing remains available. Diagnostics explicitly identify unavailable dispatch/neighbour predicate traces on 26.51.

Terrain recipes, kernel **2.211**, sea level **Y100** and building range **Y-64..511** are unchanged. Existing supported recipes remain pinned. Known issue: first entry and exploration can still show loading stalls or visible chunk gaps; this compatibility update does not establish that those problems are resolved.

Windows **0.3.0**, BDS terrain **1.0.54** / structures **1.0.69**.

## 0.2.10 — first-entry priority and structure lock contention

The player’s complete nearby dependency halo now stays ahead of older distant jobs. This corrects the 0.2.9 priority inversion that could delay first entry while view-distance work aged. Extra native scheduling checks run only after a sampled queue remains idle, rather than while native tasks are already running.

Ordinary chunks reuse bounded, value-only structure reservation snapshots without acquiring the native placement lock. Proven rejected structure candidates can also finish the adapter stage without that lock; unknown or accepted candidates retain the original planning and placement paths. Expected unfinished dependency rings at the view boundary are excluded from interior backlog pressure; enclosed holes are still tracked.

Diagnostics retain the longest lock holders and a bounded startup/recent timeline using wall time. Terrain generation rules, existing worlds, Y100 sea level and Y-64..511 build height are unchanged. Loading stalls and visible gaps can still occur during exploration.

Windows **0.2.10**, BDS terrain **1.0.53** / structures **1.0.68**, kernel **2.211** (unchanged). Existing supported worlds can be used.

## 0.2.9 — chunk scheduling fairness and structure-lock latency

Dependency-ready chunks now gain priority as they wait, including visible chunks outside the former local-only boost. Native ready queues receive a bounded scheduling check even after the nearby 3×3 has loaded. Supplemental views share one budget and pause while native queues or postprocessing are backed up. Existing native dependency checks and chunk states remain authoritative.

Cold regional water/ice planning runs on independent workers before acquiring the structure registry lock. Bounded diagnostics record queued stages, native dispatch results, failed neighbour-stage predicates and slow structure-lock sites. Stall timers discount long gaps between game ticks. The terrain recipe, saved chunks, sea level Y100, build range Y-64..511 and Y500 cloud changes are preserved; this runtime update also applies to existing supported worlds.

Known issue: exploration can still produce loading stalls or visible gaps.

Windows **0.2.9**, BDS terrain **1.0.52** / structures **1.0.67**, kernel **2.211** (unchanged). No world reset is required.

## 0.2.8 — glacier spire shape and cold-region ecology

New-world glacier spires regain slender bases and pronounced lean, correcting the incomplete height-only change in 0.2.4. Trees, shrubs and ground plants thin continuously toward super glaciers. Freezing a snowy wetland removes lilies whose water support becomes ice. Both hosts restrict natural polar-bear spawning to frozen oceans and glacier ice, excluding frozen lakes; commands, eggs, spawners and existing bears are unaffected.

Generation changes require a new recipe/world; existing immutable recipes and saved chunks are preserved. Sea level Y100, building range Y-64..511 and the 0.2.7 Y500 cloud-rendering change remain. Chunk-loading stalls and visible gaps can still occur during exploration.

Windows **0.2.8**, BDS terrain **1.0.51** / structures **1.0.66**, kernel **2.211**. Offline regressions cover real-seed spire descriptors, voxel tip displacement, connectivity and chunk order, vegetation density, warm/frozen lily support, spawn policy and old-recipe output compatibility.

## 0.2.7 — Windows cloud rendering at Y500

Fixes clouds staying at their native altitude despite the old Y500 setting. The renderer recalculated cloud placement from the dimension getter, bypassing the cached value previously changed by the mod. This update shifts the actual cloud render translation to Y500 and adjusts upper/lower mesh selection at the same altitude for original graphics and Vibrant Visuals.

Diagnostics now report the native and corrected output world heights separately for each graphics mode. Native surface bias and cloud thickness remain, so the cloud base is approximately Y500.33. This is a Windows rendering change; existing supported worlds can be used without regeneration. Terrain remains at kernel 2.210, sea level Y100 and building range Y-64..511; BDS stays at terrain 1.0.50 / structures 1.0.65.

## 0.2.6 — Windows native pack-source path identity

- Fix the mixed path separators that made Windows new-world creation report a missing native behavior-pack source. Bedrock registers forward-slash UTF-8 paths and compares them byte for byte; filesystem path joining produced a backslash before `behavior_packs`.
- Use the same UTF-8 path format when binding and looking up the world source, record both lookup and registered paths, and retain the height validation and safe failed-attachment retry introduced in 0.2.5.
- Avoid an additional repository refresh after `addWorldResourcePacks`, which already performs that operation in the pinned client.

The cause was confirmed by read-only inspection of the failing 0.2.5 client: the source existed, its discovery latch was set, and its registered path used forward slashes.  BDS stays at terrain 1.0.50 / structures 1.0.65; terrain kernel 2.210 and generation rules are unchanged.

## 0.2.5 — Windows new-world height-pack discovery and retry

- Rearm discovery for the new world's behavior-pack directory after native pack copying and before repository refresh. A directory scanned before the height pack was written must not keep an empty cached result.
- If height-pack handoff fails before native server startup, preserve this job's unchanged terrain attachment in its creation-task backup. A retry can attach again; initialized worlds, changed payloads and unrelated pack selections are protected.
- Record pack discovery state and the actual failure phase in the creation task. Keep height-resource and runtime-height checks in place.

Windows **0.2.5**. BDS remains terrain **1.0.50** / structures **1.0.65**, kernel **2.210**; generation rules, sea level and building limits are unchanged. Source is maintained in both repositories. World creation depends on native resource-pack discovery as well as the generated files.

## 0.2.4 — regional planning stalls and tall glacier spires

- Connect native structure queries to the same regional-plan disk cache as terrain generation. Exclude only structure policy fields from terrain cache identity; avoid repeating drainage planning while holding the shared structure lock.
- Sample lake surveys on demand and reuse river survey columns. Clip conifer crowns before expensive per-voxel work, hoist repeated broadleaf/birch calculations, and reuse jungle shapes within each serial vegetation worker.
- Restore new-world large glacier spires to the 88–159-block design range, retaining wider bases and the build ceiling. Existing recipes preserve their original spires.
- Record actual native load, structure, decoration, lighting and Windows mesh-builder durations, separately from polled stage residence. Add structure-lock and regional-cache statistics. These timings do not measure GPU upload or visibility.

Windows **0.2.4**, BDS terrain **1.0.50** / structures **1.0.65**, kernel **2.210**. Sea level Y100 and build range Y-64..511 are unchanged. Visible gaps and forest-loading stalls can still occur.

## 0.2.3 — 2026-10-27

- Recover stalled chunk jobs and enclosed holes without generating the untouched view frontier; keep one repair halo per player.
- Add Windows queue, compute, fill, native stage and tick diagnostics. Mesh/network timings are not inferred from native chunk readiness.
- Filter jungle vine candidates before sorting and skip empty structure vegetation reservations. Preserve terrain output.
- Target Y500 clouds in both classic and Vibrant Visuals rendering for expanded Overworlds.
- BDS terrain 1.0.49 / structures 1.0.64. Android remains archived. Forest chunk loading can still stall during exploration.

## 0.2.2 — Windows local-world startup

- Register the new world's pack source before loading its height behavior pack; include the exact height pack identity in native `LevelSettings` as well as the saved pack-selection file.
- Read and validate the overworld resource selected by the native server pack manager before activating terrain generation. Missing/conflicting resources reject startup before native chunk jobs can begin.
- Do not release the terrain session immediately after native initialization returns failure: native queued work still belongs to the level until engine teardown.
- Add persistent startup-phase diagnostics and remove stale error text after successful initialization.
- Correct the bundled installation README's stale version and sea-level text.

Windows **0.2.2**; BDS remains terrain **1.0.48** / structures **1.0.63**; kernel **2.209** and generation recipes are unchanged. This is a Windows adapter fix, maintained in both source trees. No new BDS world is needed. Android remains archived.

Evidence from the reported 0.2.1 failure: native height -64..319 vs saved -64..511, zero generated chunks/players/ticks, followed by `closed-world-request` after early session release. The same height behavior pack works in isolated BDS with experimental toggles both disabled and enabled.

## 0.2.1 — visible chunk recovery and inland landforms

- Recovery scans the player's requested view (up to 64 chunks), instead of stopping at 32. Two disjoint 5×5 repair cores retain eight dependency rings each. Distant repairs no longer count as nearby player backlog and suppress their own follow-up. Repair lifetime is 45 seconds with bounded retries; no incomplete chunk is marked loaded.
- Basins have wider floors and irregular outlines. Inland basin slopes no longer inherit a sea-level beach ring. The catalogue still uses 133 for a representative basin, not a guarantee that all basins share one size.
- Entries 134–136 now cut deep, narrow, asymmetric surface canyons. Width and depth vary along curved paths; a surveyed inland canyon may descend below sea level without being treated as ocean.
- Jungle trees gain 20–30 blocks within the build ceiling. Raised mountain interiors gain two local cave levels, irregular branches and descending connections to the deep network; lowland cave geometry is preserved.
- Lakes accept quantized local minima and larger closed depressions, with a higher regional cap. Surface lava pools use the same sealed, up-to-three-lobe layout as water pools, at a lower selection probability, with a one-block stone border. Village candidate spacing is 16 chunks instead of 20; footprint safety checks remain in place. Candidate density is not an observed village count.

Windows **0.2.1**, BDS terrain **1.0.48** / structures **1.0.63**, kernel **2.209**. Sea level remains Y100 and building range Y-64..511. New generation uses immutable `elevationRevision=2`, `smallPoolsVersion=3`, and `cave.uplandNetworks=1`; supported older recipes retain their rules. Android remains archived.

Known issue: chunk-loading stalls and visible gaps can still occur. Create a new world to use the changed terrain generation rules.

本版修正可见区块恢复的范围与积压计数，扩大并打散盆地、移除盆缘海滩环，将三档裂谷调整为深窄峡谷。丛林树增加 20–30 格，补充山体上层洞网与连接；增加湖泊和村庄候选机会，岩浆坑复用水坑封闭布局并增加石边。保留旧配方与旧世界。

## 0.2.0 — basins, altitude variants and snowy landscapes

- New worlds use sea level **Y100**. Land rises with a continuous profile while the existing peak range and **Y-64..511** building limits remain.
- Inland basins have **Y60–100** floors, surrounding drainage divides and independent local water levels. Low, middle and high altitude variants preserve their parent landscape; the transition bands overlap instead of forming height steps. Plateaus remain above Y150.
- Added small, medium and large rifts with curved axes, asymmetric sides, tapered ends and solid floors.
- Every parent biome can receive a snow skin. Glaciers have a fully snowy collar extending about 32 chunks beyond their edge, fading over the next 32 chunks. Hot volcanic cores stay clear; cold outskirts can retain snow.
- Broadened ice-spire roots and reduced excessive lean and height. Ice-body geometry no longer follows the patchy surface-material selector.
- Volcanically coated rock touching coastal water can form irregular obsidian patches.
- Visible chunk recovery now includes its dependency halo in loading priority. A stalled request can retry twice with backoff; immediate player surroundings still come first.

Use a new world to experience the new generation. Existing supported recipes keep their original rules.

新世界海面 Y100、内陆盆地 Y60～100、连续海拔变种、三档裂谷、各群系覆雪和冰川外围雪带；改善冰尖峰根部与倾斜，火山海岸增加黑曜石接触带。可见区块恢复加入依赖优先级和有间隔的有限重试。

## 0.1.19 — kernel and world-session foundations

- BDS, Windows LL and the packaged CLI now link the same SDK-free terrain library. The library owns immutable recipes and hides the Engine/Field implementation behind a facade.
- Windows compiles its entry point, terrain host and structure host separately. World setup, status, hooks and release operations use explicit adapter interfaces.
- Regional plans and cache-directory bindings belong to a world context. Terrain sessions own calculators, worker queues, chunk caches, the cache writer and latency statistics. Closing drains admitted mod work before releasing these resources; nested admitted callbacks can finish.
- Added world/recipe lifetime, same-seed context isolation, shutdown/worker-queue/cache-flush tests and a reproducible byte/latency comparison tool. Windows world counters reset on re-entry. Running BDS structure runtimes now refuse hot-disable, matching the terrain runtime.
- CI uploads the actual manifest-derived package name instead of the obsolete fixed 0.1.5 filename.

Terrain recipe **2.207** is unchanged: this release adds no terrain redesign. The existing native-write locks and fatal protection against persisting unfinished chunks remain. This is the first foundation stage; internal algorithm modules and remaining native structure state still need incremental separation. Offline timing is not a measurement of server-to-client loading, and no loading-speed improvement is claimed.

BDS terrain **1.0.46**, structures **1.0.61**; Windows **0.1.19**. Sea level Y64 and build range Y-64..511 remain. Android stays archived.

本版整理共享内核、Windows 接口边界和世界会话资源，不改地形配方。缓存、计算器与统计按世界隔离，关闭前等待已接收任务完成；保留原生写入锁和防止半成品区块落盘的保护。完整结构状态拆分和服务器加载性能优化尚未完成。

## 0.1.18 — volcano scale and world creation

- Reduced typical crater radii to 18–42 blocks, with a 5% large category of 45–58 blocks before boundary variation. The previous revision widened every crater.
- Raised lava to 7–10 blocks below the lowest sampled rim. Bent, asymmetric underground lobes and rock shelves replace the regular funnel profile; the connected magma core, bedrock seal, deepslate and diamond rules remain.
- Windows creation no longer treats an empty pre-start pack-repository snapshot as a failed world. Pack files and selection are still checked, and the actual Overworld height must match before joining.
- Existing 0.1.17 and earlier supported recipes keep their generation. New volcano rules apply to new worlds. Sea level remains Y64; building remains Y-64..511. BDS and Windows LL share the terrain changes; Android remains archived.

常见火山口缩小，少量保留大型个体；岩浆面靠近最低口缘，地下改为偏转、不对称的膨大与岩壁。Windows 取消原生加载完成前的包仓库误拦截，仍校验文件、选包及进入世界前的实际高度。

## 0.1.17 — volcano magma body and Windows loading

- Volcanoes now have a broad lava mouth connected to one continuous magma body that widens towards the world bottom. The bedrock floor and a solid rock seal remain intact.
- Fixed Windows mod startup rejecting the supported game after LeviLauncher added its loader section. Version, code-section and instruction checks remain active, and diagnostics now state the failing check.
- New worlds use the revised volcano. Existing supported world recipes, including 0.1.16, keep their original generation. Existing vanilla worlds are not converted.

Sea level remains Y64 and the build range remains Y-64..511.

火山改为从宽阔熔岩口连续向地下扩大的岩浆体，保留基岩封底。修复 Windows LL 因启动器增加加载段而被误判为不兼容、模组未加载的问题。新火山用于新建世界；已有受支持模组存档保留原规则，原版存档不自动转换。

## 0.1.16 — rebuilt from 0.1.13 / 从 0.1.13 重新实现

- Sea level Y64; raised land, continuous foothills and five mountain height bands, with a higher mountain snow belt. Build range remains Y-64..511.
- Removed the barren outer volcanic ring. Added a curved, flaring conduit, finite underground chambers, deepslate and tuff. Diamond generation gains independent extra attempts towards the centre, capped at twice the normal attempt rate and retaining normal height/host/exposure rules.
- Sparse surface moss patches and occasional azaleas above actual lush cave columns.
- Village candidate density 2.56×, with terrain suitability checks and at least 272 blocks between candidate anchors.
- Teleport recovery watches dependency-stage progress. Busy ticks suspend optional preloading; at most one gap-repair window is active.
- Default README is English; Chinese documentation remains available.

BDS and Windows LL share these rules. Android remains archived. Existing 0.1.13 and earlier supported recipes retain their terrain; experimental 0.1.14/0.1.15 (kernel 2.204) worlds require their retained matching release and are not silently migrated.

The three reported seed-15 coordinates did not show persisted missing solid blocks in the saved snapshot. The cause of the reported gap remains undetermined.

本次基于 0.1.13 重做。海面 Y64、分级山地和高原衔接、雪线、火山地下、苔藓与杜鹃、村庄密度、传送恢复均同步两端。新测试世界使用种子 18，旧世界保留。种子 15 的三处疑似永久缺块尚未复现，原因未确定。

## 0.1.15 / 0.1.14 (archived experiments / 已撤回的实验方向)

Historical packages and Git tags are retained for rollback. Version 0.1.16 restores the 0.1.13 source baseline before implementing the changes again; these experimental worlds are not automatically converted.

## 0.1.13

- 新增宽阔缓起伏高原，主要位于 Y180～300，边缘连续缓坡衔接低地与高山；保留海面 Y40、约 Y480 的峰顶和 Y511 建造上限。
- 高原采用共享地形采样，河谷沿新地势生成，植被随高原权重向疏林过渡。既有存档配方保持原规则，新世界才启用高原。
- /karma 新增 [132] 高原，目的地检查周围地势与干燥落点，保留原编号与可选模式；入服蓝字提示本次变更。
- BDS 与 Windows LL 同步；Android 封档。

## 0.1.12

- 修复新高度世界中洞群在 Y319 被截断的问题：掩码、材料写入及查询同步支持 Y511，保留自然洞顶和旧配方。
- 山地植被采用与雪线一致的地表生态温度和等效海拔，针叶林、阔叶林、白桦林及其他树型按统一树线渐变；山地灌木与地被随山体适配。
- 高山地下补充岩脉、洞穴生态、小型密闭水池和水洞，扩展怪物房候选高度，补全覆雪山体的绿宝石资格；原版矿物高度分布、深层熔岩和低地规则保留。
- 修复无限世界旧范围以外超级冰川的专属天气判断；诊断版本号与实际安装清单统一，并加入回归校验。
- 海面 Y40、峰顶约 Y480、建造范围 Y-64..511、服务端视距48保持。新规则仅随新世界配方启用，旧存档不自动迁移。
- BDS 与 Windows LL 同步，Android 继续封档。

## 0.1.11

- 山地覆雪带随山峰抬升后的高度调整，低海拔冰雪群系保留原规则；海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持。
- 区域规划改为按区域共享计算结果，不再在完整规划期间持有全局锁；仅规划影响当前采样点的相邻区域，保留逐格河岸校验。
- 进服、传送时优先完成脚下 3×3 区块；就近区块未就绪或计算拥堵时，暂缓额外的前方预加载和远处缺口修复，保留八区块依赖范围。
- BDS 默认视距上限为 48；实际视距取服务端上限与客户端请求的较小值。
- BDS 与 Windows LL 共享修复；新配方仅用于新世界，旧存档不自动迁移。安卓保持封档。

## 0.1.10

- 新增 MIT 协议，项目原创代码采用 MIT；第三方及上游衍生内容保留各自许可和声明。
- 修复河流水位高于天然岸边：规划时逐格检查河道及岸边，支流优先寻找更低的汇流点，避免形成裸露水带。
- 修复恶地内部被冰川规则切出草地、沙带的问题；统一干旱气候，侵蚀石柱的高度在水系规划前计算，保留陶瓦高原和陡坡。
- 修复 `/karma tp 111` 把恶地边缘小块沙地当作沙漠：目的地及周围 128 格范围取样，排除过渡沙带和积水；旧目录会重建。
- 共享修复覆盖 BDS 和 Windows LL；新世界启用新配方，旧存档不回填或删除区块。海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持，安卓封档。
- 增加种子 11 的边界坐标与区块输出回归。

- 修复探索期间结构缓存回收与待处理实体任务之间的失效指针风险；回收和引用清理统一在结构阶段锁内完成。
- BDS `/karma tp <序号> [模式]`：模式可省略，省略时保持当前模式；1 为创造、0 为观察者。移除 `/karma back` 及退出时自动返回。
- 新世界的水坑水面和坑底上升一格，部分水坑连成两至三个；每个水体共用水位，并检查岸边闭合。
- 陶瓦地形增加高原抬升、层状阶地和陡峭崖面；沙漠、恶地外围增加干湿缓冲，避免紧邻丛林、沼泽等湿润群系。
- 区域河网 v3 给原本共线的河段增加贝塞尔弯曲，保留汇流坐标和连续水位。
- BDS 与 Windows LL 共用生成与结构修复；海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持。旧世界沿用原生成配方；安卓封档。

# 更新日志

## 0.1.6 / BDS 指令组件 1.0.48 — 2026-10-23

- `/karma` 显示完整群系/结构目录；`/karma tp <序号> <模式>` 要求两个整数，1 为创造、0 为观察者，保留稳定编号和 `/karma back`。
- 玩家进入时用蓝字显示最近更新地形的编号和内容；缺少坐标的目录条目仍可见。
- Windows LL 同步构建，生成规则保持 0.1.5；该测试命令由 BDS 提供，未新增 Windows 单机命令。
- 隔离运行当前 BDS，确认维度配置可把范围从 -64..319 扩至 -64..511，正常重启后保持。此版本仅包含高度范围实验，尚未加入发布包。详见 [高度方案](docs/HEIGHT-FEASIBILITY.md)。
- Android 已停止维护。

## Windows LL 0.1.0 — 2026-10-19

- Windows 客户端启用独立版本号，当前版本定为 0.1.0；BDS 版本号不变。
- 继承 Windows 1.0.40 的地形和结构规则，支持普通新世界、自定义种子与随探索生成。
- 提供 LeviLauncher 可直接导入的完整 ZIP，包含种子生成运行文件、安装说明和依赖许可。
- 兼容 Minecraft 1.26.20.04 / LeviLamina 26.20.7 客户端版。

## 1.0.42 — 2026-10-19（BDS 测试功能）

- 修复关闭命令反馈时 `/karma tp` 不显示列表的问题；结构和群系列表直接发给执行者，分批显示。
- 控制台仍使用命令输出；传送编号、已保存坐标和世界生成规则不变。

## 1.0.41 — 2026-10-18（BDS 测试功能）

- `/karma tp` 增加群系传送，每类一个位置，编号从 101 开始；原有结构编号保持不变。
- 普通冰川、超级冰川、火山、小型石峰林、巨木森林单独列出。
- 空服时分批搜索并保存进度，未找到的项目显示状态；支持 `/karma back`。
- 本次仅更新 BDS 测试菜单，保留现有世界和种子，不改变生成规则。

## 1.0.40 — 2026-10-18

- 修正入口附近坡地将整栋建筑抬高的问题，入口按自身地面定位，外围保留可通行坡度。
- 栅栏门不再被识别为地面，修正围栏庭院下沉一格的问题。
- BDS 与 Windows LL 使用相同规则。BDS 新测试世界使用种子 2，保留旧世界。

## 1.0.39 — 2026-10-18

- BDS 与 Windows LL 使用同一结构规划流程；旧种子配置不再切回旧结构分支。
- 村庄地基排除植被，建筑按可通行边沿适配地面，避免内部水坑抬高整栋建筑。
- 篝火保留地面支撑；道路使用规划后的高度，移除依据放置结果再次降低路面的处理。
- 修整操作按所属区块索引，保留房屋内部空气和结构方块。

已生成的区块不会自动重建。

## [1.0.27-a](https://github.com/troveminecraft/karma-worldgen/releases/tag/v1.0.27-a) — 2026-10-16

首个 GitHub BDS 预发布包，包含地形生成、原版结构组件和新世界模板。

- 山地、沙漠、火山、冰川、河流与湖泊，以及森林和地下洞穴。
- 原版结构接入及道路、地基和周边地形适配。
- 支持 BDS `1.26.20.5`、LeviLamina `26.20.7`，运行于支持 AVX2 的 Windows x64 设备。
- 包内地形种子为 `288493980`，X/Z 范围为 `-20000` 至 `19999`。

此包用于新世界，不包含 Windows 客户端或 Android 安装包。安装步骤见[安装指南](docs/INSTALL.md)，使用前请阅读[已知问题](docs/KNOWN_ISSUES.md)。
