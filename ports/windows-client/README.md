# 山&海 · mountains&seas 0.3.3

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.51.01**、LeviLamina **26.51.9 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft；旧版升级请先按下面的迁移步骤处理。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.3.3.zip`。
3. 确认 **mountains&seas** 已启用，作者为 **troveminecraft**，版本为 **0.3.3**。
4. 启动游戏，创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含完整种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包，不要只复制 DLL。

## 本版变化与升级

修复 Minecraft **1.26.51.01** 进入世界时因种子读取失败而退出：此前把存档 NBT 的字段名用于运行时查询；现在地形和结构统一读取原生 64 位种子，并在世界初始化后核对创建设置。可选诊断写入失败也不会再阻断世界退出清理。

LeviLauncher 现在显示 **mountains&seas**，作者为 **troveminecraft**。安装目录改为 `mods/mountains&seas`，DLL 文件名及世界标记保持兼容。从 0.3.2 或更早版本升级时，完全退出 Minecraft，备份并将旧 `karma-worldgen-client` 文件夹移到 `mods` 外，再导入新 ZIP。把备份中的 `client-config.json` 和可选的 `seed-cache` 复制到新目录；不要同时启用两份模组。

适用于 LeviLamina **26.51.9 客户端版**、Windows x64 与 AVX2 CPU。内核 **2.211**、地形配方、海平面 **Y100** 和建造范围 **Y-64..511** 不变。首次进入与探索中的加载停顿和可见缺块仍是已知问题；原版世界不会自动转换。

加载问题可附模组目录的 `diagnostics/world-session.json`、`diagnostics/world-seed.json`、`diagnostics/world-startup.json` 和 `diagnostics/world-error.txt`。创建失败可附创建任务的 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json`；结构失败请同时附世界的 `karma/structures/structure-failure.json` 和 `karma/fatal-runtime.json`。

本包仅用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[mountains&seas](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
