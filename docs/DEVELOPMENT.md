# 从源码构建

## 环境

- Windows x64、Visual Studio C++ 构建工具、clang-cl、xmake。
- 26.20 使用 LeviLamina `26.20.7`；Windows 26.51 使用客户端 SDK `26.51.9`。两条游戏线分别构建，不混用 SDK。
- Python `3.13`、NumPy；客户端运行资源还需要 Node.js `22`。

## 构建

在仓库根目录执行：

```powershell
# BDS 地形与结构组件
./ports/build.ps1 -Target bds

# Windows 客户端 DLL
./ports/windows-client/build.ps1 -GameLine 26.51
```

BDS 输出分别在 `build/windows/x64/release/` 和 `experiments/vanilla-structure-runtime/build/windows/x64/release/`；客户端 DLL 在 `ports/windows-client/build/windows/x64/release/`。

完整客户端包还需要原生生成工具和运行资源：先构建 xmake 目标 `native-worldgen-check`，再使用 `ports/windows-client/build_runtime.py` 组装运行目录，最后用 `ports/package_windows.py` 打包。各脚本的 `--help` 列出必需参数。

GitHub Actions 提供手动构建入口，见 [build.yml](../.github/workflows/build.yml)。云端完整打包流程仍待验证。

## 代码位置

| 路径 | 用途 |
| --- | --- |
| `src/native/` | 地形、水系、洞穴、矿石和植被 |
| `src/rules/` | 共享结构规则 |
| `src/persistence/` | 结构数据保存 |
| `experiments/vanilla-structure-runtime/` | BDS 原版结构接口 |
| `ports/windows-client/` | 客户端游戏接口、世界创建和种子服务 |
| `tools/` | 种子、区域数据生成工具 |
| `tests/` | 规则测试和固定输入 |
| `data/` | 种子模板、矿石规则和模型数据 |
| `ports/android/` | 停止维护的 Android 源码 |

地形内核和结构接口使用 C++。客户端创建世界时，由 Python 种子服务调用区域生成工具，通过 Node.js 执行 `tools/seed/build.mjs` 准备种子配置，再调用原生内核。

`data/runtime-template.json` 是运行格式和方块表模板；`tests/fixtures/void-world/level.dat` 是创建世界所需的最小模板，不含玩家数据或区块。

## 共享规则与发布

26.20 的共享地形和结构改动先在 BDS 验证，再构建对应 Windows 模组。26.51 专属修订仅构建 Windows 26.51，不要求额外更新 BDS。平台适配层负责游戏接口、世界生命周期和安装；通用生成规则放在共享代码中。

26.20 的 `ports/build.ps1 -Target windows-mod` 通过 `-BdsAcceptance` 指定匹配源码版本的 BDS 测试记录。仅影响客户端的改动可使用 `-Scope mod-specific -Feature <功能名>`；允许的功能列在 `ports/product-policy.json`。

规则测试位于 `tests/`，平台接口测试位于对应的 `ports/` 子目录。测试方式随修改范围选择；发布步骤见[发布说明](RELEASE.md)。

Android 最后版本为 `1.0.0-dev.21`。复现旧构建需要通过 `ports/scripts/setup_android.py` 准备外部依赖，常规构建不包含 Android。

构建中间文件、离线测试输出和个人配置不提交源码。当前包的来源应同时记录源码提交、内核修订和包校验值；相同产品版本号不代表相同开发修订。
