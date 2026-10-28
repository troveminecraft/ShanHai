# 山&海 0.2.7

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.20.04**、LeviLamina **26.20.7 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.2.7.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

修复云层仍停留在原生高度的问题：旧实现只改了高度缓存，但原生渲染会重新读取维度高度，绕过该缓存。本版直接修正最终云位置，并同步调整云的上下表面选择，覆盖普通画面和灵动视效。

诊断分别记录两种模式的原生高度与修正后的输出高度。保留原生表面偏移和云层厚度，云底约为 Y500.33。此次为 Windows 渲染改动，已有受支持世界可直接使用，无需重新生成。内核 2.210、海平面 Y100、建造范围 Y-64..511 及 BDS 地形 1.0.50 / 结构 1.0.65 不变；画面效果仍待游戏内验收。

海平面 Y100，主世界建造范围 Y-64..511；请保留世界附带的高度行为包。普通和灵动视效云层仍以 Y500 为目标。旧配方和已生成区块不改写，新冰刺规则仅用于新配方。受支持的旧世界可继续使用；0.1.14/0.1.15 实验世界需使用留存的对应版本，不自动迁移。原版世界不自动转换，模组世界应保持模组启用。

这是待用户游戏内验收的测试版本。离线检查不能证明全部缺块和加载停顿已解决。更新前备份世界；反馈请附版本、种子、坐标和复现过程。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
