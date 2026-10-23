# 建造高度扩展可行性（0.1.5 调查）

目标是突破 Y=319 上方的建造限制。当前结论：有进一步验证的引擎入口，但本版没有启用扩高；不能通过仅修改配置或一个高度常量完成。

## 已确认

- 当前共享生成器固定采用 -64..319，共 384 格。`Field::materialColumn` 会拒绝高度 320；`tests/completed_biomes.cpp` 实际调用生产路径并验证拒绝。区块编码、材质缓存、洞穴位图、装饰和两端填充循环也依赖这个范围。直接放宽一个检查会使其余路径不一致。
- 当前使用的 LeviLamina 26.20.7 SDK 中，`DimensionHeightRange` 上下界为 `short`，维度定义 `Bounds` 为两个 `int`，`LevelChunk` 暴露动态高度范围和子区块范围查询。数字类型本身足以表达 512，但这不是游戏允许放置、正确保存或正确显示的证明。
- 同一 SDK 存在 `CustomDimension`、`DimensionDataPacket`，以及 `DimensionDefinition::Components` 中的 bounds/generation 字段。可以继续调查自定义维度注册、尺寸同步路径。
- Microsoft 当前文档 [minecraft:bounds](https://learn.microsoft.com/en-us/minecraft/creator/reference/content/dimensionreference/examples/dimensioncomponents/bounds?view=minecraft-bedrock-stable) 描述自定义维度的上下界参数 -512..512；[minecraft:dimensionheight](https://learn.microsoft.com/en-us/minecraft/creator/reference/content/dimensionreference/examples/dimensioncomponents/dimensionheight?view=minecraft-bedrock-stable) 描述总高度 16..1024。两页更新于 2026-10-15。当前 26.20.7 的上述 Components 只看到了 bounds/generation，不能把较新文档视为该版本已经支持新组件的证据，也不能把自定义维度支持推论为主世界支持。

## 可行路线与下一项实验

优先在独立测试维度尝试 `min=-64, max=512` 的范围（max 是否为排他上界需要实测），先只验证 Y=319、320、383、511、512 的边界，再讨论更高范围。需验证：定义能否加载；服务端与客户端收到相同高度；玩家手工放置、破坏、碰撞；跨子区块与区块边界；重登、正常保存重启后的方块保留；光照、流体和视距行为。BDS 连接客户端与 Windows LL 本地世界分别验证。

若自定义维度路径可用，下一步才将共享生成器的高度布局封装为不可变 `minY/height/maxExclusive`，区分新格式与旧存档解码，并逐项移除 384/319 的硬编码。仅把主世界的维度字段改大，无法证明固定主世界规则、数据包、客户端、存档和原生结构全都支持扩高。

本轮没有注入主世界高度、修改历史存档或启动实验世界，没有执行游戏命令、自动放置方块。已完成的是源码/SDK 可行性调查及当前边界的可执行验证；实际突破高度仍未验证，不能作为已完成功能交付。
