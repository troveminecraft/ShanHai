# 山&海 0.2.6

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.20.04**、LeviLamina **26.20.7 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.2.6.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

修复 Windows 创建世界时，混合路径分隔符导致原生行为包来源查找失败的问题。统一原生 UTF-8 路径，新增查找与来源路径诊断；保留高度检查和失败后重试保护。地形规则沿用 0.2.4。

海平面 Y100，主世界建造范围 Y-64..511；请保留世界附带的高度行为包。普通和灵动视效云层仍以 Y500 为目标。旧配方和已生成区块不改写，新冰刺规则仅用于新配方。受支持的旧世界可继续使用；0.1.14/0.1.15 实验世界需使用留存的对应版本，不自动迁移。原版世界不自动转换，模组世界应保持模组启用。

这是待用户游戏内验收的测试版本。离线检查不能证明全部缺块和加载停顿已解决。更新前备份世界；反馈请附版本、种子、坐标和复现过程。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
