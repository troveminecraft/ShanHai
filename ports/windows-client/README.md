# mountains&seas · 山&海 0.3.11

Minecraft 基岩版 Windows 地形与结构生成模组。作者：troveminecraft。

## 兼容版本

| Minecraft for Windows | LeviLamina client | 安装包 |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.11.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.11.zip` |

需要 Windows x64 和支持 AVX2 的 CPU。请选择对应游戏版本的 ZIP。

## 安装与升级

保存并完全退出 Minecraft，在 LeviLauncher 对应实例导入完整 ZIP，启用 **mountains&seas**。创建普通新世界并填写种子；不要选择超平坦。ZIP 包含 Python 和 Node.js 依赖。

从旧 `karma-worldgen-client` 条目升级时，将旧文件夹备份到 `mods` 之外后导入新包，保留 `client-config.json` 和已有存档；不要同时启用两份模组。已生成的方块保留，原版世界不会自动转换。

## 0.3.11 更新

陆地冰体限定在超级冰川区域。其他寒冷群系保留覆雪变种和水面结冰，不再因低温噪声斑块额外铺设浮冰地表。

山地悬崖核心在**水平2格内形成100～300格落差**，瀑布落差为**60～180格**。江流按沿程汇水量确定宽度；合适的陡坡基岩河段可形成带岩唇、掏蚀凹槽、下游峡槽及跌水潭的退蚀瀑布，并保留河岸、湖盆的挡水约束。

海平面保持 **Y100**，建造范围 **Y-64..511**。内核 **2.216** 保留旧世界生成配方；这些生成调整在新世界启用，不重写已有方块。密集地形、大视距或内存压力下的加载停顿仍是已知问题。

## 诊断

日志按世界会话保存，包含后台心跳、正在执行的任务及丛林缓存统计。退出游戏后运行模组目录的 `collect-diagnostics.cmd` 可生成本地诊断包；不会自动上传。详见 `LOADING_DIAGNOSTICS.md`。

原创代码采用 MIT 协议；第三方许可见 `THIRD_PARTY_NOTICES.md`。
