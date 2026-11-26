#pragma once

#include <algorithm>
#include <cstdint>

namespace bomber {
// [01] Canvas dimensions and a centered viewport that preserves their aspect ratio.
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 900;

struct Viewport {
    int x, y, width, height;
};
struct CanvasPoint { float x, y; };

inline Viewport fitViewport(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    int fittedWidth = width, fittedHeight = height;
    if (std::int64_t(width) * CanvasHeight > std::int64_t(height) * CanvasWidth)
        fittedWidth = std::max(1, int(std::int64_t(height) * CanvasWidth / CanvasHeight));
    else
        fittedHeight = std::max(1, int(std::int64_t(width) * CanvasHeight / CanvasWidth));
    return {(width - fittedWidth) / 2, (height - fittedHeight) / 2, fittedWidth, fittedHeight};
}
} // namespace bomber
