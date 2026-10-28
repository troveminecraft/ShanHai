# 山&海 0.2.10

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.20.04**、LeviLamina **26.20.7 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.2.10.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

玩家近邻的完整依赖范围保持优先，修正 0.2.10 中远处旧任务等待提权后可能压过出生点任务的问题。额外原生调度检查只在队列持续空闲时触发，避免在原生任务运行期间继续追加调度检查。

普通区块读取有界的结构范围快照，不再为读取已确定的植被预留范围等待原生结构放置总锁。已经确定没有结构的候选也可直接完成适配阶段；未知或有结构的候选仍走原来的规划和放置流程。视距边缘正常等待邻区的区块不再计入内部积压，周围已就绪的真实缺口仍会跟踪。

诊断保留耗时最长的持锁位置，以及按真实时间记录的首次加载与近期进展。地形生成规则、现有世界、海平面 Y100、建造范围 Y-64..511 不变。本版仍待用户游戏内验收，离线检查不能代表实际提速或已消除全部停顿。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
