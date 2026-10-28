# 山&海 0.3.1

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.51.01**、LeviLamina **26.51.9 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.3.1.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

修复 0.3.0 中 DLL 显示已加载、生成器却启用失败，导致新世界生成原版地形的问题。创建世界接口现在先于云层补丁启用，避免把自身修改误判为不兼容的游戏程序。保留严格版本校验，并在日志与 `diagnostics/activation.json` 中区分 DLL 加载和生成器启用状态。

适用于 Minecraft **1.26.51.01** 与 LeviLamina **26.51.9 客户端版**。地形配方、内核 **2.211**、海平面 **Y100** 与建造范围 **Y-64..511** 不变。启用失败期间创建的世界仍是原版世界，更新后请创建普通新世界。

已知问题：首次进入与探索中仍可能出现加载停顿或可见缺块，本次更新不宣称解决这些独立的性能问题。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
