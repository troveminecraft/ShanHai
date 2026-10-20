#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace terrain_runtime {
// LeviLamina/PreLoader returns zero on success. Roll back earlier detours if
// any symbol cannot be installed; a half-enabled adapter is not usable.
class HookTransaction {
    std::vector<std::function<void()>> undo;
    bool committed = false;

  public:
    HookTransaction() = default;
    HookTransaction(HookTransaction const &) = delete;
    ~HookTransaction() {
        if (!committed)
            for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
                try {
                    (*it)();
                } catch (...) {
                }
            }
    }
    template <class Hook> void add(std::string_view name) {
        // Allocate rollback storage before installing the hook.
        undo.emplace_back([] { Hook::unhook(); });
        auto error = Hook::hook();
        if (error != 0) {
            undo.pop_back();
            throw std::runtime_error("Required game hook failed: " + std::string(name) + " (" +
                                     std::to_string(error) + ")");
        }
    }
    void onRollback(std::function<void()> action) {
        undo.push_back(std::move(action));
    }
    void commit() {
        committed = true;
    }
};
} // namespace terrain_runtime
