# 主世界高度探针

仅用于隔离 BDS 1.26.20 / LeviLamina 26.20.7 测试目录。使用 xmake 构建 height-range-probe，将 DLL 与 manifest.json 放入隔离服务器的同名插件目录。不要混入山&海运行包。

先在全新虚空世界无行为包启动，读取服务器目录的 height-range-result.json。正常停服，在另一个新世界启用 behavior-pack（世界 world_behavior_packs.json 必须引用包清单的 UUID 和版本），读取结果；正常停服并重启复查。

探针在主线程首次服务器更新时实例化主世界维度，记录上下界、子区块数量和边界判断。不发送游戏命令、传送玩家、写方块或请求区块加载。范围通过不等于客户端与方块保存通过。

行为包使原版新区域生成虚空。当前山&海仍拒绝扩展高度，完成共享填充适配前只能用于隔离实验。见 ../../docs/HEIGHT-FEASIBILITY.md。
