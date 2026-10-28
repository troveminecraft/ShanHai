#pragma once
#include "HookTransaction.h"
#include <utility>

namespace client::activation {
// Creation validates the untouched executable, including cloud call sites.
// Installing rendering detours first would make our own image check fail.
template<class StartCreation, class StopCreation, class InstallRuntime>
void install(terrain_runtime::HookTransaction& hooks, StartCreation&& start,
             StopCreation&& stop, InstallRuntime&& runtime) {
    hooks.onRollback(std::forward<StopCreation>(stop));
    start();
    runtime(hooks);
}
}
