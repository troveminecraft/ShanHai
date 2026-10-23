# 建造高度扩展可行性（2026-10-23 实机引擎检查）

**存在适用于当前 BDS 1.26.20 的可行入口：行为包的主世界 `minecraft:dimension_bounds`。** 隔离运行同一份服务器程序，设置 `min=-64, max=512` 后，实际范围变为 Y=-64..511，并在正常保存重启后保持。尚未接入山&海生成器，未验收玩家放置、客户端显示和方块保存。

## 当前版本的实际结果

使用与运行中 BDS SHA-256 相同的程序、LeviLamina 26.20.7、新建隔离世界。只加载范围诊断插件，未加载山&海、未连接玩家、未执行游戏命令或放置方块。插件在服务器更新线程中实例化主世界维度并读取原生范围，不修改维度字段，不请求区块生成。每次都正常 `stop` 保存退出。

| 项目 | 无高度包 | 加载高度包 | 加载后正常重启 |
| --- | --- | --- | --- |
| 游戏构建 | 1.26.20+1f6d1b1 | 同左 | 同左 |
| 最低 Y | -64 | -64 | -64 |
| 排他上界 | 320 | 512 | 512 |
| 子区块数量 | 24 | 36 | 36 |
| Y=319 在范围内 | 是 | 是 | 是 |
| Y=320、383、511 在范围内 | 否 | 是 | 是 |
| Y=-65、512 在范围内 | 否 | 否 | 否 |

这是当前引擎已接受扩高的证据，**不是放置方块后重启仍保留的证据**，实验没有方块写入。

## 建议实施：先扩建造空间，再考虑高山生成

1. 新测试世界使用 `experiments/height-range-probe/behavior-pack/dimensions/overworld.json`：`minecraft:overworld`、`minecraft:dimension_bounds` 的 -64/512、`minecraft:generation` 的 `void`。原生新区域会变为虚空；山&海本来使用自己的生成器，这与接入方向相符，但尚未做组合运行验证。
2. 两端共用的 `src/Worldgen.cpp::fill` 当前明确拒绝非 -64/320 维度，并按固定 384 高度创建 BlockVolume 和群系子区块。因此不能直接把此行为包装进正在使用的山&海世界。
3. 最小改造是区分“生成高度 384”和“可建造高度 576”：保留 -64..319 生成配方、洞穴位图及旧缓存编码；原生区块填充采用实际维度高度，将 Y=320..511 补为空气，群系体积扩至 36 个子区块，再重算高度图。先提供额外 192 格建造空间，不必同时重做全部高山、洞穴和结构生成。
4. 高度应作为新世界持久配置，并通过行为包/维度信息正确同步给客户端。Windows 新世界准备流程也需安装该包并保存世界包引用；只改 BDS 字段不够。旧世界继续采用 384 格高度与旧配方，避免隐式迁移。
5. 若后续要求山峰也生成到 Y=320 以上，再独立扩展共享内核、材质列、洞穴位图、缓存格式与结构边界。这与“允许玩家在上方建造”是不同阶段。

## 仍需验证

- Windows 1.26.20.04 连接 BDS 后看到相同范围，Y=320、383、511 可放置/破坏，Y=512 正确拒绝。
- 方块碰撞、光照、流体，以及跨子区块、跨区块的建筑。
- 实际放置方块在重登和正常保存重启后保留。
- BDS 山&海填充适配与 Windows LL 本地世界分别验证。本次未启动 Windows 游戏或安装客户端包。

## 来源与版本边界

[Microsoft：Data Driven Overworld Height and Void Generation](https://learn.microsoft.com/en-us/minecraft/creator/documents/datadrivenoverworldheight?view=minecraft-bedrock-stable) 给出主世界维度行为包格式、16 格倍数和 -512..512 参数范围。本次验证该旧版主世界入口，未把新文档的 `minecraft:dimension_height` 当成当前构建的功能。

[Microsoft：自定义维度组件](https://learn.microsoft.com/en-us/minecraft/creator/reference/content/dimensionreference/examples/dimensioncomponents/components?view=minecraft-bedrock-stable) 属于另一组随版本演进的接口；[MoreDimensions 更新记录](https://github.com/LiteLDev/MoreDimensions/blob/develop/CHANGELOG.md) 的新版适配目标也不同。自定义维度可以作为备选，但当前主世界目标无需先引入它或升级 SDK。

实验源码和最小行为包保留在 `experiments/height-range-probe`，不包含在模组运行包中。当前 BDS Seed8 与 Windows 发布包均未启用扩高。
