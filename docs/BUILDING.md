# 构建 Windows LL 模组

需要 Windows x64、Visual Studio 2026 C++ Build Tools（含 LLVM/clang-cl）、xmake 3.1.1、
Python 3.13（安装 NumPy）和 Node.js 24。将 `xmake`、`clang-cl`、`python`、`node` 加入 PATH。
使用完整 Node.js 官方发行包，保留 `node.exe` 旁的 `LICENSE` 文件，打包时会一同带入。
构建器会获取 LeviLamina **26.20.7 客户端 SDK**、LeviBuildScript 和 nlohmann/json **3.12.0**。

在仓库根目录运行：

```powershell
python -m pip install numpy
./build.ps1 -OutputDirectory ../shan-hai-build
```

输出目录包含：

- `client/windows/x64/release/karma-worldgen-client.dll`：客户端模组。
- `kernel/windows/x64/release/native-worldgen-check.exe`：种子生成所需的计算程序。
- `runtime/`：打包后的种子生成依赖。
- `ShanHai-Windows-<manifest.version>.zip`：完整安装包。

安装时使用 ZIP，单独的 DLL 缺少运行依赖。主线推送会运行 GitHub Actions，也可手动运行 **Windows LL** 构建工作流；
构建产物不会自动发布到 Releases。

## 源码布局

| 目录 | 用途 |
| --- | --- |
| `src/kernel`、`src/TerrainKernel.h` | 独立静态内核库与公开接口，BDS / Windows / CLI 共同使用 |
| `src/native` | 内核内部的地形、植被、矿石与洞穴实现 |
| `src/rules` | 结构选址、地基、道路和放置规则 |
| `src` | 区块生成、缓存、调度和运行时接入 |
| `experiments/vanilla-structure-runtime` | 编译进客户端的结构运行时，保留原目录以维持包含关系 |
| `ports/windows-client` | Windows 游戏接口、新建世界与种子服务 |
| `tools` | 种子与水系准备，包括 Python 和 JavaScript 依赖 |
| `data` | 生成规则模板 |
| `tests` | 内核及结构回归检查；空白世界模板用于创建世界 |

源码中的共享实现保留服务端兼容代码，但本仓库不发布服务端插件或安卓安装包。
修改地形规则前应了解缓存标识及已有区块的持久化行为。

## 检查

结构生命周期、布局检查点、钩子回滚与缓存检查：

```powershell
./tests/run.ps1 -BuildDirectory ../shan-hai-tests -OutputDirectory ../shan-hai-results
```

默认入口用仓内模板与 Node.js 重建三个输入（5、-1、有符号 64 位最大种子），运行列边界、调色板、错误上下文、创建进度、植被预留和既有生命周期测试，再比较两个独立计算器以相反顺序生成的区块及缓存重访结果。另运行 9 项 Python 进程与超时测试。

测试输入为离线内核配方，使用种子默认地标；不模拟原生出生点选择。`host-tests.json` 记录完成的用例；测试中的失败注入只是报错路径验证，不代表自然生成失败率。

世界封装检查不需要启动游戏：

```powershell
$env:PYTHONPATH = (Join-Path $PWD "tools")
python -B ports/windows-client/test_world_package.py
```

`build_runtime.py` 会使用打包的 Python 解释器检查运行时导入；
`package_windows.py` 会逐项核对运行时文件的 SHA-256。

内核与世界会话边界、测试范围见 [KERNEL_BOUNDARY.md](KERNEL_BOUNDARY.md)。
