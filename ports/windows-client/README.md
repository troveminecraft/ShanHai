# mountains&seas · 山&海 0.3.6

Minecraft 基岩版 Windows 地形与结构生成模组。作者：troveminecraft。

## 兼容版本

| Minecraft for Windows | LeviLamina client | 安装包 |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.6.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.6.zip` |

需要 Windows x64 和支持 AVX2 的 CPU。请选择对应游戏版本的 ZIP。

## 安装与升级

保存并完全退出 Minecraft，在 LeviLauncher 对应实例导入完整 ZIP，启用 **mountains&seas**。创建普通新世界并填写种子；不要选择超平坦。ZIP 包含 Python 和 Node.js 依赖。

从旧 `karma-worldgen-client` 条目升级时，将旧文件夹备份到 `mods` 之外后导入新包，保留 `client-config.json` 和已有存档；不要同时启用两份模组。已生成的方块保留，原版世界不会自动转换。

## 0.3.6 更新

同一世界的计算器共享已完成的丛林树形；同时请求同一棵树时只构建一次，缓存优先保留最近使用的内容，并限制数量及保留内存。

一组 1,089 个丛林周边区块的离线测试中，生成耗时从 19.2 秒降至 9.6 秒，所有区块数据逐字节一致。这是地形内核测试，完整游戏加载时间还包含装饰、邻区依赖、光照和渲染；部分加载停顿仍在调查。

海平面 Y100，建造范围 Y-64..511。延续 0.3.5 的 26.51 怪物房初始化修复及持续加载诊断。

## 诊断

日志按世界会话保存，包含后台心跳、正在执行的任务及丛林缓存统计。退出游戏后运行模组目录的 `collect-diagnostics.cmd` 可生成本地诊断包；不会自动上传。详见 `LOADING_DIAGNOSTICS.md`。

原创代码采用 MIT 协议；第三方许可见 `THIRD_PARTY_NOTICES.md`。
