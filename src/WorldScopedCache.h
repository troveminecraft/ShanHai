#pragma once
#include <optional>
#include <utility>

// Cached game objects belong to a level's registry. Reset before that registry
// is destroyed, even when the next world has the same seed or display name.
template<class T> class WorldScopedCache {
    std::optional<T> value;
public:
    template<class Factory> T& get(Factory&& factory) {
        if (!value) value.emplace(std::forward<Factory>(factory)());
        return *value;
    }
    void reset() { value.reset(); }
};
