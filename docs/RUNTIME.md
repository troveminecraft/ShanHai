# 生成与存档

## 0.3.9 河岸、湖泊与高差悬崖

新世界按相邻水柱的实际高度保留河岸，并在河道削坡时保护封闭湖泊的岸线。跨区域河道规划共享所经过区域的湖泊，使用实测湖盆低点作为终点，拒绝以不同水位切穿湖岸的路线。

山地悬崖核心可在**水平 2 格内形成 60～100 格落差**；河流可沿合适的 **30～100 格陡坎**形成相连瀑布。悬崖两端及外围缓接原地形。海平面保持 **Y100**，建造范围保持 **Y-64..511**。

内核 **2.215** 保留已有世界的生成配方。本次生成调整在新世界启用，不重写已有方块。通过有界地表缓存及按路线需要读取湖盆，限制重复采样。密集地形、大视距或内存压力下仍可能加载缓慢，离线检查不能代表完整游戏加载表现。

## 0.3.8 高原冰川与河流

新世界的超级冰川冰体与细长、倾斜的大型冰刺可延伸至寒冷高原。冰川外围覆雪及植被影响在 **32 区块（512 格）** 内完全衰减；该范围以外由自然寒冷气候产生的积雪保持原样。

高原河流可沿实际陡坎形成连续瀑布，仍须接入真实湖泊、海洋或下游河网，不能倒流至出口，也不能在低岸上悬空。增加已有坡面上的悬崖，候选海拔上限扩展至 **Y420**。海平面保持 **Y100**，建造范围保持 **Y-64..511**。

内核 **2.214** 保留已有世界存储的生成配方。本次地形调整需在新世界体验，不重写已有方块。保留此前的区块阶段调度与丛林树形缓存；密集地形、大视距或内存压力下仍可能加载缓慢，离线生成检查不能代表完整游戏加载表现。

## 0.3.7 区块调度与海岸连续性

区块调度保留跨生成阶段的等待记录，限制旧请求的优先级加成，并优先推进已就绪的结构、装饰和光照任务，让附近地形更早完成。修正玩家位于小数坐标、负坐标时近邻范围少覆盖一侧的问题；原生邻区依赖检查仍然有效。

新世界使用内核 **2.213**，海岸高度抬升与地表材质的影响逐渐减弱，修复旧高度阈值过早截断造成的突降。已有世界保留存档中的配方及方块，海岸修正需在新世界体验。海平面仍为 **Y100**，建造范围仍为 **Y-64..511**。

修正渲染计时将客户端世界与服务端世界地址混比、导致采样为空的问题。种子运行时不再导入测试模块，安装包排除 Python/NumPy 测试扩展与字节码缓存。密集地形、较大视距或内存压力下仍可能加载缓慢；离线依赖流程测试不等于完整游戏加载测试。

## 0.3.4 加载与湖泊

修正 Minecraft 1.26.51.01 的区块完成状态识别，避免已完成区块被计入积压并关闭预读。拥有至少 12 个逻辑处理器的 CPU 使用四个有界地形计算槽，其他 CPU 保留两个。结构邻区读取复用有界地形列缓存，诊断日志在释放原生结构锁后写入。

区域湖泊按逐格地形验证连通岸线，修复水位以下相邻地面突然断水的硬切边；湖床改用连续沉积分区。内核更新为 **2.212**，海平面仍为 **Y100**，建造范围仍为 **Y-64..511**。

已生成区块保留。湖泊修正作用于受支持区域世界的新生成地形，与旧区块交界处可能仍有接缝。更新前请备份世界。密集地形和较大视距下仍可能加载缓慢，本版不宣称消除了所有停顿。原版世界不会自动转换。

区块诊断的状态名称与缺块观察使用统一阶段布局；原生任务 trace 保留原生编号，并记录 nativeLoadedState。渲染计时 sampled=false 表示没有捕获样本，不代表耗时为零。原生结构对象仍保留必要的串行访问。

## 0.1.4 水系与石峰林

新世界的 `regional-drainage-v2` 配方使用三次贝塞尔河道和可汇入的细支流，河网在区域规划时计算，逐方块生成仍查询分段河槽。曲线控制柄限制在相邻采样边长的 28%，每段采样弦长不超过 6 格；汇流节点保留同一坐标和水位。河道只接受有限深度的地形切削，不用抬高地面制造挡水堤。

旧世界继续读取自身的 `regional-drainage-v1` 配方与石峰林尺寸，新旧配方的区域缓存身份不同。内核支持 2.193、2.194 和 2.195 配方；不要手动改旧世界的算法字段或复制新配方覆盖旧配方。已保存区块不重写，新生成规则在新世界中启用。

离线回归覆盖几何连续性、汇流连接、水位下降、河道中心取样与生成顺序一致性。

山&海用空的超平坦生成器承载自定义主世界，在区块生成阶段填入地形，再调用游戏中的结构与模板接口。村庄、战利品、实体和结构查询仍复用原版接口。空超平坦是本项目的实现选择，不代表其他方式都不可行。

## 版本边界

Windows 分别适配 Minecraft 1.26.20.04 / LeviLamina 26.20.7，以及 Minecraft 1.26.51.01 / LeviLamina 26.51.9 客户端版。创建世界的接口包含经过该版本校验的布局适配；游戏升级后需要重新核对，不能直接扩大兼容版本号。客户端启用前核对游戏映像，必要钩子安装失败时撤销已安装的钩子。

装饰区块的中心通过游戏的区块查询接口确定，不读取 `GridArea + 0xc8`。查询结果必须组成完整的 3×3 邻区，且中心处于正确的装饰阶段。

## 并发与缓存

原版结构对象和共享规划器有可变状态，仍需要串行访问。`stageMutex` 保护这些对象，统计等待时间和持锁时间，不能把等待耗时混作计算耗时。

植被预留先在锁内复制占地，再在锁外计算。两个工作槽各有独立的地形缓存，只修改调用者自己的方块缓冲区；槽满时等待，不无限创建计算器。旧区块和玩家修改不经过这条清理路径。

客户端与 BDS 共用区块加载观测、近处优先和缺块恢复逻辑。恢复不会把未完成区块强行标为已加载。

结构缓存定期分批检查，保留仍有区块引用的结构，包括正在生成的区块。通过原版结构缓存回收接口释放离开的区域，再清理适配层引用。待生成实体的任务先保存坐标，再释放其旧结构指针；区域重载时重新取得结构对象。预留完成提示最多保留 4096 项，丢弃提示只会触发重新计算。

这不意味着所有内存恒定：当前加载的区块、实体、游戏注册表，以及存档中的持久化索引仍随实际内容变化。运行状态会记录结构缓存数量、回收量、锁耗时与区块恢复统计，便于区分缓存滞留和正常世界数据。

## 持久化与失败

世界的种子、地形配方与文件校验用于避免错接数据和新区块接缝。展示名称可以修改；改变地形规则需要明确的兼容或迁移方案，不应删除校验后直接继续生成。

- 方块与玩家改动以游戏存档为准，已经保存的区块不会因重建结构记录而重新放置。
- 实体有独立的持久化声明，先写盘再生成，防止重进后重复生成。化石另有完整方块计划记录。
- 0.1.1 为村庄、散布结构和其他大型结构增加布局检查点。首次规划记录边界、部件和关键落位信息；重新规划不一致时停止继续放置，不覆盖旧记录。村庄还记录模板选择、旋转、位置和适配地面。
- 检查点不是完整 Minecraft 结构序列化，也不是“区块已保存”的标记。原版对象仍按同一配方重建后核对。0.1.0 没有这些检查点，无法追溯证明它已经生成的布局与首次升级重算一致。
- 结构阶段失败不再关闭结构生成后返回成功。装饰阶段暂停并记录具体错误；地形缓冲区处理失败沿用停止生成进程的保护，避免保存不完整区块。

兼容范围应列出准确的 Minecraft 构建和加载器版本；构建与离线测试的结果应注明测试范围。

## 0.1.2 故障定位与兼容边界

地形失败写入 `karma/fatal-runtime.json`（BDS 在地形插件目录），结构失败写入结构诊断目录的 `structure-failure.json`。记录包含组件版本、世界、完整种子、阶段、原因；已确定区块时记录坐标，未确定时明确为 null。原有文本错误文件继续保留。

创建准备失败的具体原因显示在界面，并保存于模组目录 `creation-jobs/<任务>/failure.json` 与 `worker.log`。种子配方步骤最多 120 秒，地标采样最多 300 秒，配方子进程最多 600 秒；出生点单次请求最多 120 秒、整段查询最多 900 秒；客户端准备总时限 30 分钟。超时只停止本次准备的所属进程，不创建失败地形或自动进入世界。

0.1.2 不改世界身份和结构检查点格式，保留既有 2.193/2.194 配方支持。内部缓存修订号使派生地形缓存重新计算，不删除游戏世界的已保存区块。越界修复只对尚未保存的生成结果生效，不能自动修复过去可能写错的方块。已经存在的布局检查点若与重建结果不符，仍拒绝继续。

升级前保留完整世界备份；源码提交、旧安装包不等同于世界备份。降级只在停止游戏/服务后恢复对应整包，并优先配套恢复升级前世界备份。尚未完成跨版本存档恢复演练，不能承诺对 0.1.2 已写入的新区块无损降级。

## 0.1.8 结构高度和恶地区域

结构捕获读取维度高度范围的上、下界，使用两者之差分配列缓冲；不再把 getHeight() 返回的上界当作列长。致命诊断的写入与退出串行化，避免多个工作线程留下截断记录。

新配方 badlandsRegions=1 选择大型恶地区域，使用沙漠边带隔开其他陆地群系，并在临海处铺设红沙及红砂岩支撑。特殊地貌保留，河湖水位不抬高；缺省或 0 保留旧配方。

## 0.2.4 加载瓶颈与诊断

结构适配器过去创建独立地形上下文时未绑定区域规划磁盘缓存，且修改 `sharedRules` 导致同一地形的缓存身份不同；结构查询因此可能在持有 `stageMutex` 时重复规划河湖，阻塞后续结构任务。本版绑定同一世界的缓存目录，地形缓存身份仅剔除不参与地形采样的 `sharedRules` 和 `villageSpacing`。不同种子、地形配置、模板和算法仍隔离。旧缓存不冒充新身份，首次遇到未缓存区域仍需计算。

河道复用已取样的地形列；湖泊只取实际勘测和连通检查需要的列。关闭的湖泊连通区域仍以原顺序输出。森林改动保留形状与预留规则；丛林树形仅在同一 Field、同一配方的串行植被工作器内共享，不跨并行工作器共享可变缓存。

Windows 模组的 `diagnostics/world-session.json` 记录 `nativeWork`：原生加载、结构、装饰、光照任务的实际函数耗时，以及 `clientMeshBuild`。耗时包含函数内等待；与按 tick 轮询的阶段停留时间分开。只记录最近 32 条超过 100ms 的原生慢任务坐标。网格指标不包含 GPU 上传、渲染排队或最终可见性，不能把原生就绪等同于画面显示。

结构性能记录包含锁等待/持锁延迟分布、植被预填充计算和区域缓存命中/重建统计。BDS 写入结构插件的 `performance.json`，Windows 并入会话诊断的结构状态。区域规划构建累计时间可能包含嵌套规划，不能直接相加当作独立 CPU 时间。排查时同时对照 tick 间隔、计算器排队、原生任务和网格构建。

恢复的大冰刺由新配方 `tallIceSpires=true` 启用。旧配方不修改，旧世界不会自动长高；新建模组世界使用该规则。

## 0.2.5 Windows 创建失败

真实失败记录显示：种子准备和地形接入成功，随后原生资源仓库找不到高度包，尚未调用本地服务器启动；之后在同一创建界面重试，会因残留的 `karma` 和标记文件被已有世界保护拒绝。创建失败不应误报为地形计算失败。

接入时先绑定世界包来源，取得该世界的 Behavior 目录来源，将 SDK 26.20.7 的目录发现状态重新启用，再加载/刷新仓库并检查精确包身份与主世界高度定义。操作仅涉及当前尚未启动的新世界目录，不全局启用高度包。原生阶段实际是否读到包由每个任务的 `height-pack-discovery.json` 记录；缺少来源或包仍然失败关闭，不绕过高度检查。

在这一步失败时，只将当前 Job 创建且与 prepared 数据逐文件一致的 `karma` 和标记文件移动到该任务的 `failed-attachment`。不删除世界、行为包目录或其他选择；任何 `level.dat`、LevelDB、额外文件或内容变化都会阻止此移动。允许重试仍通过原有 fresh-world 检查，而不是把任意旧标记当作可覆盖凭据。已有失败目录若已删除，不自动恢复。

本版没有改变 BDS 或地形规则。BDS 启动成功不能证明 Windows 原生包扫描成功，离线文件回归也不能代替游戏内新建与重进。

## 0.2.6 Windows 原生包路径

0.2.5 实测三次创建均在 `height-pack-before-local-server` 阶段报 `Native world behavior-pack source is missing`；失败后回退和重试已实际生效。本次只读检查了该进程的原生代码与包来源：`RepositorySources::initializeWorldPackSource` 使用 `/` 拼接目录，`PackSourceFactory::getDirectoryPackSource` 按类型、字符串长度和字节相等查找，不把斜线视为等价。实际 Behavior 来源存在，`discovered=true`、`rediscoverable=false`。

模组用 `filesystem::operator/` 拼接后，Windows 会产生末尾 `\behavior_packs`；SDK 的 `Core::Path(filesystem::path)` 直接保存 `u8string()`，不转为原生来源使用的 `/behavior_packs`。0.2.6 通过 `generic_u8string()` 统一传入仓库的 UTF-8 路径，并将 `lookupPath` 和 `sourcePath` 写入任务的 `height-pack-discovery.json`。没有往游戏进程写内存或调用游戏逻辑进行验证。

Windows 回归覆盖混合分隔符以及含空格、中文的路径，保留接入/回退/旧世界保护测试。`addWorldResourcePacks` 已调用绑定与刷新，移除随后多余的刷新。

## 0.2.7 Windows cloud translation

The previous `mCloudHeight=500` hook ran in both graphics modes, but that count was not proof of cloud placement. Offline inspection of the pinned 1.26.20.04 executable found:

- `tickClouds` (RVA `0x3AD5180`) reads camera Y at `0x708` and the dimension getter (virtual slot `0xC0`) to choose upper/lower faces. It does not use the cached cloud height for this decision.
- `_extractClouds` (RVA `0x3AD7E00`) writes `dimensionCloudY - cameraTargetY + 0.33` to `CloudRenderObject::mYTranslation` (`0x34`). Vibrant mode changes lighting, not this placement formula.
- `renderClouds` (RVA `0x3ABBB60`) uses that extracted translation in both the draw matrix and the cloud frustum bounds. The original and Vibrant paths share this transform.

The fix adds `500 - nativeHeight` to the extracted translation. During the synchronous mesh-side comparison only, an RAII scope offsets camera Y by the inverse amount and restores it on return or exception. In this pinned native method, camera Y is read only for that comparison; queued rebuilds capture the resulting side boolean and X/Z. It does not move the player, change camera target Y, force double-sided meshes or hook the ICF-shared dimension getter. SDK layout assertions and native instruction signatures guard the assumptions.

`diagnostics/world-session.json` → `clouds.classic` / `clouds.vibrant` contain sampled `nativeHeight`, `cameraY`, `nativeWorldY`, `outputWorldY`, `hasMeshes` and extraction number. Missing samples are null; the counters and samples are process-scoped. At the extraction boundary the expected output base is about 500.33, retaining native bias and thickness. These values describe the CPU-side renderer output before GPU submission. Non-finite samples are counted and left untouched. Sampling takes a lock once per 120 extractions, with no disk IO on the render path.

Offline checks replay the measured translation/face decision below, inside and above the target layer and check normal/exception camera restoration. Image tests reject changed cloud instruction signatures. Existing world creation safeguards remain enabled. BDS has no cloud renderer; its plugins, test seed and generated chunks are unchanged.

## 0.3.0 native adapter coverage

Windows now targets Minecraft **1.26.51.01** with LeviLamina **26.51.9**. World creation, height-pack discovery, chunk views, save-state tracking and native structure placement use the new game interfaces. The shared source retains a separate 26.20 build path; DLLs for different game builds are not interchangeable.

The Y500 cloud adjustment uses the two native cloud-height call sites shared by original graphics and Vibrant Visuals. Executable layout and instruction checks reject unsupported builds. Native chunk task timing follows each task’s owning chunk, and client mesh-build timing remains available. Diagnostics explicitly identify unavailable dispatch/neighbour predicate traces on 26.51.

Terrain recipes, kernel **2.211**, sea level **Y100** and building range **Y-64..511** are unchanged. Existing supported recipes remain pinned. Known issue: first entry and exploration can still show loading stalls or visible chunk gaps; this compatibility update does not establish that those problems are resolved.

The new adapter times native load, structure, decoration and lighting closures without changing their results. `nativeWork.*.available` distinguishes unsupported metrics from zero samples. `pipeline.dispatchPredicateTraceAvailable` and `pipeline.neighbourPredicateTraceAvailable` are false on 26.51 because those native helpers were inlined. Queue snapshots remain read-only and bounded; client mesh timing excludes GPU publication.

## Windows activation diagnostics (0.3.1)

`diagnostics/activation.json` separates executable validation, hook installation, enabled, failed and disabled states. `ready: true` means all required hook installers completed, not that a world has finished generating. With the module fully enabled, `newWorldsUseKarma` reflects the creation setting. Creation validation runs before rendering detours modify the pinned cloud call sites. If another installer fails, runtime hooks are restored before creation hooks are removed.

## Windows 26.51 generator constructor ABI (0.3.2)

The native VoidGenerator constructor takes the biome ID as a scalar in R8W. The SDK's `BiomeIdType` inherits `NewType`'s nontrivial copy constructor, so clang passes that wrapper indirectly. The detour uses `std::uint16_t` for its native boundary and resolves the existing constructor entry without imposing the SDK argument type. The original call and the detour share this scalar signature. Game-image checks pin both the callee's argument copy and the native zero-ID caller. The original 26.20 signature is unchanged.

`tests/void_constructor_abi.py` replays the actual compiled DLL detour using Unicorn and its PDB symbols. It can reproduce the 0.3.1 null read and verifies values 0, 1, 255, 256 and 65535, the original call's arguments, post-construction binding order and the constructor result. Native construction and binding are observed stubs: this test does not execute a game world. It requires `pefile`, `unicorn` and `llvm-pdbutil`.

```powershell
python -B -X utf8 tests/void_constructor_abi.py --dll PATH_TO_DLL --pdb PATH_TO_PDB --pdbutil PATH_TO_LLVM_PDBUTIL
```

## Windows native seed and world entry (0.3.3)

In the pinned 1.26.51.01 image, the static key initializer at RVA `0x64E48C` writes `seed` (hash `0x92EB667EA331F1E6`) to the HashedString at `0x11D2C540`. `LevelData::setSeed` at `0x621590` stores a LevelSeed64 variant (index 7); native getters at `0x1178860` and `0x11788A0` query that same key through `0x62F050`. `RandomSeed` is the NBT serialization name, not this runtime property. The adapter uses the native lookup so overrides retain their precedence. Missing or incorrectly typed entries are errors; they are not replaced with seed zero.

After native initialization, the client compares LevelData with the validated LevelSettings seed before reporting ready and writes `diagnostics/world-seed.json`. Status-write errors cannot prevent session release or the native world destructor. The creation worker, immutable recipe, height pack, void-generator detour and native template warmup remain part of the same creation-to-entry path.

`tests/native_seed_26_51.py IMAGE` replays the real native key initializer, property lookup and seed getters with synthetic map nodes. Only virtual level dispatch and byte comparison are stubbed. It reproduces the old lookup failure and checks override precedence and signed-boundary bit patterns without starting Minecraft. `bedrock-seed-test` in the client xmake project tests the production adapter with SDK seed types; `world_startup` checks seed mismatch rejection. These are offline tests, not world-entry acceptance.

The launcher reads the displayed name from `manifest.name`, and LL requires it to match the folder. The ZIP and lip placement therefore use `mountains&seas/`; the entry DLL remains `karma-worldgen-client.dll`. Old configuration and seed caches can be copied into that folder after removing the old active entry. The manifest declares the old entry as a conflict to prevent loading both together.
