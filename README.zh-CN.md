当前版本：0.2.7。修正扩展主世界云层的最终渲染位置和上下表面选择，普通画面与灵动视效均以 Y500 为基准，并记录实际输出高度。地形规则沿用 0.2.4；此次无需重新生成世界，云层画面仍待游戏内验收。

<p align="center">
  <img src="docs/assets/shan-hai.png" alt="山&amp;海" width="720">
</p>

<p align="center">
  <a href="https://github.com/troveminecraft/ShanHai/releases/tag/v0.2.7"><img src="https://img.shields.io/badge/release-0.2.7-2ea44f" alt="0.2.7"></a>
  <img src="https://img.shields.io/badge/Minecraft_Bedrock-1.26.20.04-62b47a" alt="Minecraft Bedrock 1.26.20.04">
  <img src="https://img.shields.io/badge/LeviLamina-26.20.7-00a896" alt="LeviLamina 26.20.7">
  <img src="https://img.shields.io/badge/Windows-x64-0078d4" alt="Windows x64">
</p>

<p align="center">
  <a href="https://github.com/troveminecraft/ShanHai/releases/tag/v0.2.7">下载安装</a> ·
  <a href="docs/BUILDING.md">从源码构建</a> ·
  <a href="https://github.com/troveminecraft/ShanHai/issues">反馈问题</a>
</p>

# 山&海

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

需要 **Minecraft for Windows 1.26.20.04**、**LeviLamina 26.20.7 客户端版**和支持 AVX2 的 Windows x64 电脑。可通过 LeviLauncher 管理游戏实例和模组。

1. 下载 [ShanHai-Windows-0.2.7.zip](https://github.com/troveminecraft/ShanHai/releases/download/v0.2.7/ShanHai-Windows-0.2.7.zip)。
2. 完全退出 Minecraft，在 LeviLauncher 对应实例的模组管理中导入 ZIP。
3. 确认模组已启用，启动游戏，创建一个普通新世界；不要选择超平坦。

请下载 Releases 中的安装包，GitHub 自动生成的 **Source code** 是源码。安装包已带齐运行依赖，无需另装 Python 或 Node.js，也不要只复制 DLL。

模组的内部目录与标识仍为 `karma-worldgen-client`，用于保持存档和配置兼容。它是“山&海”的同一个模组。

本次更新见 [更新记录](CHANGELOG.md)，生成和存档机制见 [说明](docs/RUNTIME.md)。

## 0.2.7 更新

修复云层仍停留在原生高度的问题：旧实现只改了高度缓存，但原生渲染会重新读取维度高度，绕过该缓存。本版直接修正最终云位置，并同步调整云的上下表面选择，覆盖普通画面和灵动视效。

诊断分别记录两种模式的原生高度与修正后的输出高度。保留原生表面偏移和云层厚度，云底约为 Y500.33。此次为 Windows 渲染改动，已有受支持世界可直接使用，无需重新生成。内核 2.210、海平面 Y100、建造范围 Y-64..511 及 BDS 地形 1.0.50 / 结构 1.0.65 不变；画面效果仍待游戏内验收。

## 使用前

**0.2.7 是公开测试版本。** 结构与复杂坡地的衔接仍可能出现异常，欢迎带上种子、坐标和截图反馈。建议备份存档，并用新世界体验；已经生成的原版区块不会自动变成新地形。

继续游玩模组世界时，请保持模组启用。这个安装包仅用于上面列出的 Windows 客户端环境，不适用于 BDS、Android 或 Java 版。

## 源码

本仓库包含 0.2.7 的地形内核、结构生成、Windows 接口和构建脚本。种子生成需要用到 Python 与 JavaScript，它们也是模组的一部分。构建方法见 [BUILDING.md](docs/BUILDING.md)。

第三方组件及许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。项目原创代码采用 [MIT 协议](LICENSE)；第三方组件及派生资源保留各自许可。安装包不包含 Minecraft 游戏本体或原版结构资源，结构模板从本机游戏读取。

本系列基于 0.1.13 的源码重新实现：新世界海面 Y100、Y60～100 内陆盆地、海拔与覆雪变种、三档裂谷和火山海岸接触带。0.1.14/0.1.15 实验世界请使用留存的对应版本，不静默迁移。游戏内验收尚待用户完成。

已有 0.1.16 模组世界保持原配方，不重写已有区块。日志应显示 `Karma Windows client 0.2.7; local world creation enabled`，随后模组启用成功；若启用失败，请连同日志与 `diagnostics/game-image.json` 反馈。
