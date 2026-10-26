# Terrain kernel and world ownership

The 0.1.19 foundation stage preserves terrain recipe 2.207. It is an incremental change, not a completed replacement of the native structure architecture.

`src/kernel/xmake.lua` builds the SDK-free `terrain-kernel` static library. BDS terrain, BDS structures, Windows LL and `native-worldgen-check` link it. `TerrainKernel.h` exposes values and operations; `src/native/Engine.h` and `Field.h` remain internal algorithm implementations. Direct implementation access in regression tests is intentional, to compare the compiled facade against the previous entry point.

A `terrain_kernel::Context` owns regional plans, in-flight deduplication and disk-cache directory bindings. Different worlds may have identical seeds without sharing mutable cache residency. Each Field/Generator owns its recipe; generators retain a supplied Field's implementation even if its original handle is destroyed. Fields and generators are single-worker objects. A context may be shared by independent workers. `RegionalPlanStore.h` uses a scoped thread-local binding only to route nested synchronous algorithm calls to the explicit owner; its default store is for internal algorithm tests.

Windows `client.cpp`, `Worldgen.cpp` and the structure adapter compile separately. `WindowsWorldHost.h` defines orchestration calls; the entry point no longer includes implementation `.cpp` files or reaches into adapter globals.

`TerrainSession` owns the terrain worker pool, generators, decoded chunk cache, same-chunk locks, cache writer and latency measurements. World shutdown closes admission, drains admitted mod jobs, flushes accepted derived-cache writes, releases generators and clears cache ownership before native registry references are released. Nested callbacks on an admitted thread may finish. `WorldTasks::Lease` is confined to the thread that acquires it. It is not Bedrock's task scheduler, and this barrier does not claim to cancel all native jobs. Native world-close ordering still requires gameplay validation.

Native structure writes remain protected by the existing stage lock. Prefill and decoration are tracked before resources are released. BDS refuses live hot-disable of either runtime. Generation errors that could leave partially written chunks retain the existing stop/pause protection; returning an empty chunk or marking failed decoration complete would corrupt saved generation state. Reducing the scope of these failures requires transactional native-write/rollback evidence and is not completed here.

The remaining work includes smaller internal algorithm modules, further ownership of native structure caches/registries, scoped recovery for proven rollback-safe failures, and measured BDS scheduling/loading optimization. A static library or more translation units alone does not make runtime generation faster.

## Reproducible checks

Run `tests/run.ps1` for the host suite. `kernel_facade`, `regional_plan_concurrency`, `world_tasks` and `terrain_session` cover owned recipe/Field lifetimes, context isolation, parallel deduplication, shutdown admission, two-worker queue draining and accepted cache-write completion. The static-lifetime regression also closes an already-drained owner after thread-local destructors have run; lease tracking uses a trivial thread-local head rather than a destructible container. These are offline tests, not a Minecraft acceptance run.

To compare two binaries, give both the same immutable recipe and separate new output directories:

```powershell
python tests/kernel_baseline.py --exe path/to/old.exe --recipe path/to/native-input.json --output ../baseline-old
python tests/kernel_baseline.py --exe path/to/new.exe --recipe path/to/native-input.json --output ../baseline-new --compare ../baseline-old/result.json
```

The report hashes the recipe, binary and 12 complete chunks, records 289 surface probes and verifies repeated generation. Timings identify the CLI process scope; they exclude server scheduling, native filling, lighting, networking and rendering. Keep hardware, compiler flags and competing workloads comparable. Do not infer in-game loading speed from these numbers.
