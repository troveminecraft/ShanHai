#pragma once
#include <cmath>

namespace client::clouds {
inline constexpr short Height = 500;

inline bool appliesToDimension(int id, int maxYExclusive) {
    return id == 0 && maxYExclusive == 512;
}

inline float translatedCloudY(float nativeTranslation, short nativeHeight) {
    // Preserve the engine's camera-relative translation and its surface bias.
    return nativeTranslation + float(Height - nativeHeight);
}

// In the pinned client, tickClouds reads camera Y only to select the mesh's
// upper/lower faces against Dimension::getCloudHeight()+1. Its asynchronous
// rebuild captures that boolean and X/Z, not camera Y. Translate that comparison
// into the native altitude frame, then restore the camera before returning.
class ScopedCloudCameraY {
    float& cameraY;
    float original;
public:
    ScopedCloudCameraY(float& y, short nativeHeight) : cameraY(y), original(y) {
        if (std::isfinite(y)) cameraY += float(nativeHeight - Height);
    }
    ~ScopedCloudCameraY() { cameraY = original; }
    ScopedCloudCameraY(ScopedCloudCameraY const&) = delete;
    ScopedCloudCameraY& operator=(ScopedCloudCameraY const&) = delete;
};
}
