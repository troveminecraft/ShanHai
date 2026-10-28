# 山&海 0.2.9

Minecraft 基岩版 Windows 客户端地形生成模组。

## 安装

需要 Minecraft for Windows **1.26.20.04**、LeviLamina **26.20.7 客户端版**、Windows x64 和支持 AVX2 的 CPU。

1. 保存并完全退出 Minecraft。
2. 在 LeviLauncher 对应实例的模组管理中导入 `ShanHai-Windows-0.2.9.zip`。
3. 确认 `karma-worldgen-client` 已启用，再启动游戏。
4. 创建普通新世界，可自行填写种子；不要选择超平坦。

ZIP 包含种子生成运行依赖，无需另装 Python 或 Node.js。请安装完整包；同标识模组应替换原安装，避免两份 DLL 同时加载。内部目录与标识保留用于兼容存档。

## 本版变化与存档

原生依赖已满足、等待处理的区块会随等待时间提高优先级，覆盖原先近处提权范围外的可见区块；脚下 3×3 就绪后，只要原生队列仍有任务，也会有界地检查调度。额外预加载与缺块恢复共用预算，队列或后处理积压时暂停新增请求，原生依赖检查和区块状态机继续负责判断是否能执行。

首次区域水系/冰川规划先由独立工作器在结构注册锁外完成，减少长规划对其他结构任务的阻塞。新增有界诊断，记录排队阶段、原生投递检查返回值、失败的邻区阶段条件及结构慢锁位置；停滞计时扣除过长的 tick 间隔。地形配方、已有区块、海平面 Y100、建造范围 Y-64..511 与 Y500 云层改动保持，本次运行时修复可用于受支持的旧世界。

这是待用户游戏内验收的测试版本。离线测试验证调度策略和地形输出一致性，不代表已消除全部可见缺块，也不能推算实际游戏加载提速比例。

加载问题可附模组目录的 `diagnostics/world-session.json`。创建失败可附 `diagnostics/world-startup.json`、`diagnostics/world-error.txt`、创建任务 `handoff.jsonl`、`failure.json`、`height-pack-discovery.json` 和世界 `karma/fatal-runtime.json`。

本包适用于上述 Windows 客户端环境。原版结构模板从本机游戏读取，不分发游戏本体。下载与反馈：[ShanHai](https://github.com/troveminecraft/ShanHai)。原创代码采用随包 MIT 协议，第三方运行库保留各自许可。
