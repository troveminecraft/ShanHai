# mountains&seas · 山&海 0.3.4

Minecraft 基岩版 Windows 地形与结构生成模组。作者：troveminecraft。

## 兼容版本

| Minecraft for Windows | LeviLamina client | Package |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.4.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.4.zip` |

Windows x64、AVX2 CPU。根据游戏版本选择 ZIP，DLL 不可混用。

## 安装

保存并完全退出 Minecraft，在 LeviLauncher 对应实例导入完整 ZIP。确认 mountains&seas 已启用，创建普通新世界并填写种子。不要选择超平坦。

从旧 karma-worldgen-client 条目升级时，先备份并将旧文件夹移出 mods，再导入新包；保留 client-config.json 和可选的 seed-cache。不要同时启用两份模组。ZIP 已包含 Python/Node.js 依赖，无需另行安装。

## 本版变化

修正 Minecraft 1.26.51.01 的区块完成状态识别，避免已完成区块被计入积压并关闭预读。拥有至少 12 个逻辑处理器的 CPU 使用四个有界地形计算槽，其他 CPU 保留两个。结构邻区读取复用有界地形列缓存，诊断日志在释放原生结构锁后写入。

区域湖泊按逐格地形验证连通岸线，修复水位以下相邻地面突然断水的硬切边；湖床改用连续沉积分区。内核更新为 **2.212**，海平面仍为 **Y100**，建造范围仍为 **Y-64..511**。

已生成区块保留。湖泊修正作用于受支持区域世界的新生成地形，与旧区块交界处可能仍有接缝。更新前请备份世界。密集地形和较大视距下仍可能加载缓慢，本版不宣称消除了所有停顿。原版世界不会自动转换。

问题反馈可提供游戏和模组版本、种子、坐标、复现步骤，以及模组 diagnostics 下的 world-session.json、world-startup.json、world-error.txt。

下载与反馈：[mountains&seas](https://github.com/troveminecraft/ShanHai)。原创代码使用 MIT 协议，第三方组件保留各自许可。安卓封档。
