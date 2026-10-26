## 0.2.1 — visible chunk recovery and inland landforms

- Recovery scans the player's requested view (up to 64 chunks), instead of stopping at 32. Two disjoint 5×5 repair cores retain eight dependency rings each. Distant repairs no longer count as nearby player backlog and suppress their own follow-up. Repair lifetime is 45 seconds with bounded retries; no incomplete chunk is marked loaded.
- Basins have wider floors and irregular outlines. Inland basin slopes no longer inherit a sea-level beach ring. The catalogue still uses 133 for a representative basin, not a guarantee that all basins share one size.
- Entries 134–136 now cut deep, narrow, asymmetric surface canyons. Width and depth vary along curved paths; a surveyed inland canyon may descend below sea level without being treated as ocean.
- Jungle trees gain 20–30 blocks within the build ceiling. Raised mountain interiors gain two local cave levels, irregular branches and descending connections to the deep network; lowland cave geometry is preserved.
- Lakes accept quantized local minima and larger closed depressions, with a higher regional cap. Surface lava pools use the same sealed, up-to-three-lobe layout as water pools, at a lower selection probability, with a one-block stone border. Village candidate spacing is 16 chunks instead of 20; footprint safety checks remain in place. Candidate density is not an observed village count.

Windows **0.2.1**, BDS terrain **1.0.48** / structures **1.0.63**, kernel **2.209**. Sea level remains Y100 and building range Y-64..511. New generation uses immutable `elevationRevision=2`, `smallPoolsVersion=3`, and `cave.uplandNetworks=1`; supported older recipes retain their rules. Android remains archived.

This is a **test release awaiting user gameplay acceptance**. Offline generation and startup checks do not establish that every visible gap or network stall has been eliminated. Use the fresh BDS Seed23 world or create a new Windows mod world for the changed terrain.

本版修正可见区块恢复的范围与积压计数，扩大并打散盆地、移除盆缘海滩环，将三档裂谷调整为深窄峡谷。丛林树增加 20–30 格，补充山体上层洞网与连接；增加湖泊和村庄候选机会，岩浆坑复用水坑封闭布局并增加石边。保留旧配方与旧世界，游戏内加载表现和地形观感由用户验收。

## 0.2.0 — basins, altitude variants and snowy landscapes

- New worlds use sea level **Y100**. Land rises with a continuous profile while the existing peak range and **Y-64..511** building limits remain.
- Inland basins have **Y60–100** floors, surrounding drainage divides and independent local water levels. Low, middle and high altitude variants preserve their parent landscape; the transition bands overlap instead of forming height steps. Plateaus remain above Y150.
- Added small, medium and large rifts with curved axes, asymmetric sides, tapered ends and solid floors.
- Every parent biome can receive a snow skin. Glaciers have a fully snowy collar extending about 32 chunks beyond their edge, fading over the next 32 chunks. Hot volcanic cores stay clear; cold outskirts can retain snow.
- Broadened ice-spire roots and reduced excessive lean and height. Ice-body geometry no longer follows the patchy surface-material selector.
- Volcanically coated rock touching coastal water can form irregular obsidian patches.
- Visible chunk recovery now includes its dependency halo in loading priority. A stalled request can retry twice with backoff; immediate player surroundings still come first.

Use a new world to experience the new generation. Existing supported recipes keep their original rules. This is a **test release awaiting gameplay acceptance**; no server-to-client loading-speed percentage is claimed.

新世界海面 Y100、内陆盆地 Y60～100、连续海拔变种、三档裂谷、各群系覆雪和冰川外围雪带；改善冰尖峰根部与倾斜，火山海岸增加黑曜石接触带。可见区块恢复加入依赖优先级和有间隔的有限重试。游戏内验收仍待用户完成。

## 0.1.19 — kernel and world-session foundations

- BDS, Windows LL and the packaged CLI now link the same SDK-free terrain library. The library owns immutable recipes and hides the Engine/Field implementation behind a facade.
- Windows compiles its entry point, terrain host and structure host separately. World setup, status, hooks and release operations use explicit adapter interfaces.
- Regional plans and cache-directory bindings belong to a world context. Terrain sessions own calculators, worker queues, chunk caches, the cache writer and latency statistics. Closing drains admitted mod work before releasing these resources; nested admitted callbacks can finish.
- Added world/recipe lifetime, same-seed context isolation, shutdown/worker-queue/cache-flush tests and a reproducible byte/latency comparison tool. Windows world counters reset on re-entry. Running BDS structure runtimes now refuse hot-disable, matching the terrain runtime.
- CI uploads the actual manifest-derived package name instead of the obsolete fixed 0.1.5 filename.

Terrain recipe **2.207** is unchanged: this release adds no terrain redesign. The existing native-write locks and fatal protection against persisting unfinished chunks remain. This is the first foundation stage; internal algorithm modules and remaining native structure state still need incremental separation. Offline timing is not a measurement of server-to-client loading, and no loading-speed improvement is claimed.

BDS terrain **1.0.46**, structures **1.0.61**; Windows **0.1.19**. Sea level Y64 and build range Y-64..511 remain. Android stays archived. Builds, offline checks and startup smoke checks do not constitute gameplay acceptance.

本版整理共享内核、Windows 接口边界和世界会话资源，不改地形配方。缓存、计算器与统计按世界隔离，关闭前等待已接收任务完成；保留原生写入锁和防止半成品区块落盘的保护。仍需用户验收进服、探索、退出重进与 Windows 创建世界；完整结构状态拆分和服务器加载性能优化尚未完成。

## 0.1.18 — volcano scale and world creation

- Reduced typical crater radii to 18–42 blocks, with a 5% large category of 45–58 blocks before boundary variation. The previous revision widened every crater.
- Raised lava to 7–10 blocks below the lowest sampled rim. Bent, asymmetric underground lobes and rock shelves replace the regular funnel profile; the connected magma core, bedrock seal, deepslate and diamond rules remain.
- Windows creation no longer treats an empty pre-start pack-repository snapshot as a failed world. Pack files and selection are still checked, and the actual Overworld height must match before joining. Actual Windows world creation remains subject to gameplay acceptance.
- Existing 0.1.17 and earlier supported recipes keep their generation. New volcano rules apply to new worlds. Sea level remains Y64; building remains Y-64..511. BDS and Windows LL share the terrain changes; Android remains archived.

常见火山口缩小，少量保留大型个体；岩浆面靠近最低口缘，地下改为偏转、不对称的膨大与岩壁。Windows 取消原生加载完成前的包仓库误拦截，仍校验文件、选包及进入世界前的实际高度。本版仍待用户游戏内验收。

# 更新记录

## 0.1.17 — volcano magma body and Windows loading

- Volcanoes now have a broad lava mouth connected to one continuous magma body that widens towards the world bottom. The bedrock floor and a solid rock seal remain intact.
- Fixed Windows mod startup rejecting the supported game after LeviLauncher added its loader section. Version, code-section and instruction checks remain active, and diagnostics now state the failing check.
- New worlds use the revised volcano. Existing supported world recipes, including 0.1.16, keep their original generation. Existing vanilla worlds are not converted.

This is a test release awaiting gameplay acceptance. Sea level remains Y64 and the build range remains Y-64..511.

火山改为从宽阔熔岩口连续向地下扩大的岩浆体，保留基岩封底。修复 Windows LL 因启动器增加加载段而被误判为不兼容、模组未加载的问题。新火山用于新建世界；已有受支持模组存档保留原规则，原版存档不自动转换。游戏内验收待用户完成。

## 0.1.16 — rebuilt from 0.1.13 / 从 0.1.13 重新实现

This is a test release awaiting user gameplay acceptance. Compilation and offline checks do not constitute in-game acceptance.

- Sea level Y64; raised land, continuous foothills and five mountain height bands, with a higher mountain snow belt. Build range remains Y-64..511.
- Removed the barren outer volcanic ring. Added a curved, flaring conduit, finite underground chambers, deepslate and tuff. Diamond generation gains independent extra attempts towards the centre, capped at twice the normal attempt rate and retaining normal height/host/exposure rules.
- Sparse surface moss patches and occasional azaleas above actual lush cave columns.
- Village candidate density 2.56×, with terrain suitability checks and at least 272 blocks between candidate anchors.
- Teleport recovery watches dependency-stage progress. Busy ticks suspend optional preloading; at most one gap-repair window is active.
- Default README is English; Chinese documentation remains available.

BDS and Windows LL share these rules. Android remains archived. Existing 0.1.13 and earlier supported recipes retain their terrain; experimental 0.1.14/0.1.15 (kernel 2.204) worlds require their retained matching release and are not silently migrated.

The three reported seed-15 coordinates did not show persisted missing solid blocks in the saved snapshot. Runtime loading remains a hypothesis requiring gameplay confirmation; this release does not claim that coordinate issue is conclusively resolved.

本次基于 0.1.13 重做。海面 Y64、分级山地和高原衔接、雪线、火山地下、苔藓与杜鹃、村庄密度、传送恢复均同步两端。新测试世界使用种子 18，旧世界保留。三处坐标的永久缺块尚未复现，不以离线检查替代游戏内验收。

## 0.1.15 / 0.1.14 (archived experiments / 已撤回的实验方向)

Historical packages and Git tags are retained for rollback. Version 0.1.16 restores the 0.1.13 source baseline before implementing the changes again; these experimental worlds are not automatically converted.

## 0.1.13（待游戏内验收）

- 新增宽阔缓起伏高原，主要位于 Y180～300，边缘连续缓坡衔接低地与高山；保留海面 Y40、约 Y480 的峰顶和 Y511 建造上限。
- 高原采用共享地形采样，河谷沿新地势生成，植被随高原权重向疏林过渡。既有存档配方保持原规则，新世界才启用高原。
- /karma 新增 [132] 高原，目的地检查周围地势与干燥落点，保留原编号与可选模式；入服蓝字提示本次变更。
- BDS 与 Windows LL 同步；Android 封档。离线生成与启动检查不代替用户游戏内验收。

## 0.1.12（待游戏内验收）

- 修复新高度世界中洞群在 Y319 被截断的问题：掩码、材料写入及查询同步支持 Y511，保留自然洞顶和旧配方。
- 山地植被采用与雪线一致的地表生态温度和等效海拔，针叶林、阔叶林、白桦林及其他树型按统一树线渐变；山地灌木与地被随山体适配。
- 高山地下补充岩脉、洞穴生态、小型密闭水池和水洞，扩展怪物房候选高度，补全覆雪山体的绿宝石资格；原版矿物高度分布、深层熔岩和低地规则保留。
- 修复无限世界旧范围以外超级冰川的专属天气判断；诊断版本号与实际安装清单统一，并加入回归校验。
- 海面 Y40、峰顶约 Y480、建造范围 Y-64..511、服务端视距48保持。新规则仅随新世界配方启用，旧存档不自动迁移。
- BDS 与 Windows LL 同步，Android 继续封档。代码、离线生成和启动检查不代替游戏内验收；自然水洞密度、植被观感及高处交互仍待用户验证。

## 0.1.11（待游戏内验收）

- 山地覆雪带随山峰抬升后的高度调整，低海拔冰雪群系保留原规则；海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持。
- 区域规划改为按区域共享计算结果，不再在完整规划期间持有全局锁；仅规划影响当前采样点的相邻区域，保留逐格河岸校验。
- 进服、传送时优先完成脚下 3×3 区块；就近区块未就绪或计算拥堵时，暂缓额外的前方预加载和远处缺口修复，保留八区块依赖范围。
- BDS 视距上限按用户要求调整为 48；实际视距仍取服务端上限与客户端请求的较小值。
- BDS 与 Windows LL 共享修复；新配方仅用于新世界，旧存档不自动迁移。安卓保持封档。游戏内速度、雪线观感和稳定性待用户验收。

## 0.1.10（待游戏内验收）

- 新增 MIT 协议，项目原创代码采用 MIT；第三方及上游衍生内容保留各自许可和声明。
- 修复河流水位高于天然岸边：规划时逐格检查河道及岸边，支流优先寻找更低的汇流点，避免形成裸露水带。
- 修复恶地内部被冰川规则切出草地、沙带的问题；统一干旱气候，侵蚀石柱的高度在水系规划前计算，保留陶瓦高原和陡坡。
- 修复 `/karma tp 111` 把恶地边缘小块沙地当作沙漠：目的地及周围 128 格范围取样，排除过渡沙带和积水；旧目录会重建。
- 共享修复覆盖 BDS 和 Windows LL；新世界启用新配方，旧存档不回填或删除区块。海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持，安卓封档。
- 回归包含 BDS 种子 11 的四处反馈坐标及实际区块方块输出；游戏内流体更新、地形观感和探索稳定性仍由用户验收。

## 0.1.9（待游戏内验收）

- 修复探索期间结构缓存回收与待处理实体任务之间的失效指针风险；回收和引用清理统一在结构阶段锁内完成。
- BDS `/karma tp <序号> [模式]`：模式可省略，省略时保持当前模式；1 为创造、0 为观察者。移除 `/karma back` 及退出时自动返回。
- 新世界的水坑水面和坑底上升一格，部分水坑连成两至三个；每个水体共用水位，并检查岸边闭合。
- 陶瓦地形增加高原抬升、层状阶地和陡峭崖面；沙漠、恶地外围增加干湿缓冲，避免紧邻丛林、沼泽等湿润群系。
- 区域河网 v3 给原本共线的河段增加贝塞尔弯曲，保留汇流坐标和连续水位。
- BDS 与 Windows LL 共用生成与结构修复；海面 Y=40、峰顶约 Y=480、建造上限 Y=511 保持。旧世界沿用原生成配方；安卓封档。
- 离线回归、双端编译和无人连接的 BDS 启动检查不代表游戏内验收。长距离探索稳定性、指令行为、流体更新和地形观感由用户验收。

## 0.1.8

- 修复探索或传送到新区域时，结构生成误读维度高度而导致退出的问题。
- 恶地扩大为连续区域，外围以沙漠过渡，与海洋相接处生成红沙海岸和近岸红沙海床。
- 保留恶地河湖水位、陶瓦条带、风蚀柱与疏林高原；旧世界继续使用原来的生成配方。
- 新世界仍为海平面 Y=40、峰顶约 Y=480、建造上限 Y=511。

## 0.1.7（待游戏内验收）

- 新世界建造上限 Y=511、峰顶约 Y=480、海平面 Y=40。
- 共享区块、植被和结构填充适配576格高度，河谷、溪流、湖泊与海岸按新高度衔接。
- 世界附带高度行为包；旧档保留旧配方与高度。
- BDS 与 Windows LL 同步实现；Windows仅交付，游戏内验收待完成。

## 0.1.6 / BDS 指令组件 1.0.48 — 2026-10-23

- `/karma` 显示完整群系/结构目录；`/karma tp <序号> <模式>` 要求两个整数，1 为创造、0 为观察者，保留稳定编号和 `/karma back`。
- 玩家进入时用蓝字显示最近更新地形的编号和内容；缺少坐标的目录条目仍可见。
- Windows LL 同步构建，生成规则保持 0.1.5；该测试命令由 BDS 提供，未新增 Windows 单机命令。
- 隔离运行当前 BDS，确认维度配置可把范围从 -64..319 扩至 -64..511，正常重启后保持。仅为可行性实验，未加入发布包，放置、客户端和方块保存尚未验收。详见 [高度方案](docs/HEIGHT-FEASIBILITY.md)。
- 指令、模式和进服显示仍待游戏内验收；安卓保持封档。

## 0.1.4

- 新建世界使用区域河网 v2：以三次贝塞尔曲线平滑河道转折，保留汇流节点，限制曲线偏移和采样间距，并按实际地形检查河槽。
- 补充山间细溪流和支流汇入，放宽适宜河源筛选；每个区域最多 6 条独立水系、14 条河源路线。支流沿已有水系汇入，共享连接点水位，沿程水位不升高。
- 封闭盆地湖泊候选上限由每区 3 处增至 4 处，仍需通过实际盆地封闭检查，不保证每区生成固定数量。
- 石峰林外缘直径约由 224 格增至 320 格，峰体半径范围从 16–22 格调至 19–26 格，间距随之增大；峰高与出现概率保持原值。
- 新旧区域河网配方分开识别与缓存。既有 v1 世界继续使用原规则和存档内参数，已保存区块不重写；新内容请创建新世界体验。
- 新增曲线转角、支流衔接、水位单调、干燥/火山排除、多种子实际河道取样和石峰林尺度回归。BDS 与 Windows LL 共用同一规则实现。

本版仍为公开测试；离线测试和构建不代表游戏内观感、流体更新或长距离运行已验收。安卓保持封档。

## 0.1.3

- 修复进入新世界时结构装饰中心取样返回空值后触发保护退出的问题；改为使用原版装饰任务传入的目标区块，并校验同一视图、邻域和区块身份。
- 装饰任务上下文按线程与调用作用域隔离，嵌套或异常退出后恢复；新增缺失上下文、视图不匹配、邻域不完整及线程隔离回归。
- BDS 与 Windows LL 同步安装任务桥接钩子，并沿用安装失败回滚和故障停止保护。

本版仍为测试。游戏内新建、进入及重进后的结构生成待用户验收。

## 0.1.2

- 修复冰雪生成在世界顶越过列边界的问题，限制下游裂隙与冰池使用的高度；新增实际生成函数的边界回归。
- 世界加载时校验完整方块名称、状态、编号和群系顺序；保留已有存档身份校验。
- 增加内部派生区块缓存修订号，旧世界不再复用修复前的区块计算缓存；不重写已经保存的游戏区块。
- 创建世界时显示准备进程的详细错误，对种子准备、出生点查询和整个创建准备设置超时并清理所属进程。
- 地形与结构故障记录组件版本、种子、区块、阶段、原因及处理建议；保留防止不完整区块保存的停止保护。
- 默认测试接入可重建的三种子输入、双计算器顺序一致性、植被预留对拍、进程超时和错误详情检查。主线推送自动执行 CI。

本版为公开测试。未实现完整写块事务或自动修复半结构；新建、重进、结构连续性、实体恢复、玩家修改保留和长距离性能仍需游戏内验收。

## 0.1.1

- 修复 Windows 客户端未启用区块加载观测和恢复的问题。
- 结构阶段失败时明确报错并阻止继续接受未完成区块，避免后续结构悄悄停止生成。
- 改用原版区块查询确定装饰中心，移除固定 `GridArea` 偏移读取。
- 将植被预留计算移出结构全局锁，使用两个独立计算器；新增锁等待与计算耗时统计。
- 增加结构布局检查点，重进后校验布局一致性；保留原有实体声明和化石记录。
- 分批回收不再使用的结构缓存，保存并恢复尚未执行的实体任务，限制预留提示缓存。
- 校验必要游戏钩子的安装结果，安装失败时回滚，避免适配层只启用一部分。
- 整理结构生命周期代码、开发说明和第三方许可边界。

仍限定 README 中列出的游戏版本。原版结构对象的操作仍有同步保护，未宣称结构生成全面并行。旧世界升级的限制见 [生成与存档](docs/RUNTIME.md)。

## 0.1.0

首个公开的 Windows LL 测试版本，包含地形、种子创建流程与原版结构接入。
