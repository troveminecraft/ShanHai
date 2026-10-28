# 山&海 · mountains&seas 0.3.2

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.51.01**、LeviLamina **26.51.9 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.3.2.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

修复 Minecraft **1.26.51.01** 加载新世界时的崩溃：地形生成器构造函数传入的是群系编号，旧钩子误将该数值作为指针读取。现在按原生调用约定传递编号，并校验对应游戏指令。保留 0.3.1 的生成器启用顺序修复。

英文名统一为 **mountains&seas**。完整 ZIP 的清单增加作者 **troveminecraft**，供 LeviLauncher 显示；原内部标识、存档路径和安装包文件名保持兼容。

适用于 LeviLamina **26.51.9 客户端版**、Windows x64 与 AVX2 CPU。地形配方、内核 **2.211**、海平面 **Y100**、建造范围 **Y-64..511** 不变。首次进入与探索中的加载停顿和可见缺块仍是已知问题。生成器未启用时创建的原版世界不会自动转换。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[mountains&seas](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
