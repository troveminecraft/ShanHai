当前版本：0.3.8。补全高原超级冰川，增加高原河流、瀑布与坡面悬崖。

<p align="center">
  <img src="docs/assets/shan-hai.png" alt="山&amp;海" width="720">
</p>

<p align="center">
  <a href="https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.8"><img src="https://img.shields.io/badge/release-0.3.8-2ea44f" alt="0.3.8"></a>
  <img src="https://img.shields.io/badge/Minecraft_Bedrock-1.26.51.01-62b47a" alt="Minecraft Bedrock 1.26.51.01">
  <img src="https://img.shields.io/badge/LeviLamina-26.51.9-00a896" alt="LeviLamina 26.51.9">
  <img src="https://img.shields.io/badge/Windows-x64-0078d4" alt="Windows x64">
</p>

<p align="center">
  <a href="https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.8">下载安装</a> ·
  <a href="docs/BUILDING.md">从源码构建</a> ·
  <a href="https://github.com/troveminecraft/ShanHai/issues">反馈问题</a>
</p>

# 山&海 · mountains&seas

[English](README.md) · 简体中文

翻过山脊，沿着河流往下走，看看下一片森林和海岸是什么样子。

**山&海**是 Minecraft 基岩版的主世界生成模组，运行于 Windows 客户端的 LeviLamina。山地、水系、森林和地下洞穴共同构成新的地貌，原版结构也会参与生成。

## 在这里可以遇见什么

- 山脉、峡谷与火山，以及连接山地、平原和海岸的河流、湖泊。
- 森林、湿地、沙漠和冰川；超级冰川中还有冰锥与深裂谷。
- 曲折的地下通道、洞室，以及地下水和深层岩浆区域。
- 村庄、沉船、神殿、古城等原版结构，按地形规则选择位置。

种子可以自行填写。创建世界时先准备附近区域，之后随探索继续生成，不设固定的测试地图边界。

## 安装

| Minecraft for Windows | LeviLamina 客户端 | 安装包 |
|---|---|---|
| 1.26.51.01 | 26.51.9 | `ShanHai-Windows-0.3.8.zip` |
| 1.26.20.04 | 26.20.7 | `ShanHai-Windows-26.20-0.3.8.zip` |


支持 **Minecraft for Windows 1.26.51.01 / LeviLamina 26.51.9** 与 **1.26.20.04 / LeviLamina 26.20.7** 两套客户端组合，需要支持 AVX2 的 Windows x64 电脑。可通过 LeviLauncher 管理游戏实例和模组。

1. 在 [0.3.8 下载页](https://github.com/troveminecraft/ShanHai/releases/tag/v0.3.8) 选择与下表游戏版本对应的 ZIP。
2. 完全退出 Minecraft，在 LeviLauncher 对应实例的模组管理中导入 ZIP。
3. 确认模组已启用，启动游戏，创建一个普通新世界；不要选择超平坦。

请下载 Releases 中的安装包，GitHub 自动生成的 **Source code** 是源码。安装包已带齐运行依赖，无需另装 Python 或 Node.js，也不要只复制 DLL。

模组管理中的名称为 **mountains&seas**，作者为 **troveminecraft**。从旧名称升级时，先备份并将 `karma-worldgen-client` 文件夹移出 `mods`，再导入新包；不要同时启用两个条目。

本次更新见 [更新记录](CHANGELOG.md)，生成和存档机制见 [说明](docs/RUNTIME.md)。

## 0.3.8 更新

新世界的超级冰川冰体与细长、倾斜的大型冰刺可延伸至寒冷高原。冰川外围覆雪及植被影响在 **32 区块（512 格）** 内完全衰减；该范围以外由自然寒冷气候产生的积雪保持原样。

高原河流可沿实际陡坎形成连续瀑布，仍须接入真实湖泊、海洋或下游河网，不能倒流至出口，也不能在低岸上悬空。增加已有坡面上的悬崖，候选海拔上限扩展至 **Y420**。海平面保持 **Y100**，建造范围保持 **Y-64..511**。

内核 **2.214** 保留已有世界存储的生成配方。本次地形调整需在新世界体验，不重写已有方块。保留此前的区块阶段调度与丛林树形缓存；密集地形、大视距或内存压力下仍可能加载缓慢，离线生成检查不能代表完整游戏加载表现。


## 使用前

**0.3.8 是公开测试版本。** 结构与复杂坡地的衔接仍可能出现异常，欢迎带上种子、坐标和截图反馈。建议备份存档，并用新世界体验；已经生成的原版区块不会自动变成新地形。

继续游玩模组世界时，请保持模组启用。这个安装包仅用于上面列出的 Windows 客户端环境，不适用于 BDS、Android 或 Java 版。

## 源码

本仓库包含 0.3.8 的地形内核、结构生成、Windows 接口和构建脚本。种子生成需要用到 Python 与 JavaScript，它们也是模组的一部分。构建方法见 [BUILDING.md](docs/BUILDING.md)。

第三方组件及许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。项目原创代码采用 [MIT 协议](LICENSE)；第三方组件及派生资源保留各自许可。安装包不包含 Minecraft 游戏本体或原版结构资源，结构模板从本机游戏读取。

本系列基于 0.1.13 的源码重新实现：新世界海面 Y100、Y60～100 内陆盆地、海拔与覆雪变种、三档裂谷和火山海岸接触带。0.1.14/0.1.15 实验世界请使用留存的对应版本，不静默迁移。

已有 0.1.16 模组世界保持原配方，不重写已有区块。日志应显示 `Karma Windows client 0.3.8; local world creation enabled`，随后模组启用成功；若启用失败，请连同日志与 `diagnostics/game-image.json` 反馈。
