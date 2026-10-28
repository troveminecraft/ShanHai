#undef NDEBUG
#include "../ports/windows-client/CloudHeightPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace client::clouds;

int main() {
    assert(appliesToDimension(0, 512));
    assert(!appliesToDimension(0, 320));
    assert(!appliesToDimension(1, 512));
    assert(!appliesToDimension(2, 512));
    // Replay the observed native extraction and mesh-side comparison, including
    // the range where the old cache-only fix selected the wrong face and height.
    int cases = 0;
    for (short native : {128, 192, 195, 500}) {
        for (float y : {-64.f, 100.f, 194.f, 250.9467f, 499.f, 500.f, 501.f, 511.f, 1024.f}) {
            float original = float(native) - y + .33f;
            auto result = translatedCloudY(original, native);
            assert(std::abs(result + y - 500.33f) < .0002f);
            assert(std::abs((result - original) - float(500-native)) < .0002f);
            float camera = y;
            {
                ScopedCloudCameraY comparison(camera, native);
                bool nativeAbove = float(native+1) <= camera;
                assert(nativeAbove == (501.f <= y));
            }
            assert(camera == y);
            ++cases;
        }
    }
    float camera = 250.9467f;
    try {
        ScopedCloudCameraY comparison(camera, 192);
        throw std::runtime_error("native failure");
    } catch (std::runtime_error const&) {}
    assert(camera == 250.9467f);
    camera = std::numeric_limits<float>::infinity();
    { ScopedCloudCameraY comparison(camera, 192); assert(std::isinf(camera)); }
    assert(std::isinf(camera));
    std::cout << "cloud height: " << cases << " native-coordinate replays passed; scope and camera restoration passed\n";
}
