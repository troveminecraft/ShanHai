# 结构运行时

本目录保留了现有源码布局。这里的结构放置、地形衔接和实体生成实现由
`ports/windows-client/client.cpp` 直接编译进客户端模组，不是可单独删除的实验脚本。

无游戏接口依赖的规则位于 `src/rules`。本仓库只提供 Windows LL 的构建入口。
