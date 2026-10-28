# 山&海 0.3.0

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.51.01**、LeviLamina **26.51.9 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.3.0.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

Windows 适配 Minecraft **1.26.51.01** 与 LeviLamina **26.51.9 客户端版**。同步调整创建世界、高度包发现、区块视图、存档脏状态及原版结构放置。共享源码保留独立的 26.20 构建入口，不同游戏版本的 DLL 不能混用。

Y500 云层改为接入原始画面与灵动视效共用的两个原生高度调用点，并校验可执行文件与指令边界。原生区块任务按实际所属区块记录耗时，保留客户端网格构建计时；26.51 暂不可采集的调度与邻区判断会在诊断中明确标出。

地形配方、内核 **2.211**、海平面 **Y100** 与建造范围 **Y-64..511** 不变。首次进入与探索中仍可能出现加载停顿或可见缺块；本次兼容更新不代表这些加载问题已经解决。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
