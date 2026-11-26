#pragma once
#include "game.hpp"
#include "viewport.hpp"
#include <cmath>

namespace bomber::speedControls {
// [01] Shared slider geometry keeps rendering and letterboxed mouse input aligned.
constexpr float Left = 865.f, Right = 1205.f, FirstTrack = 416.f, RowHeight = 64.f;
constexpr float ResetX = 862.f, ResetY = 658.f, ResetWidth = 150.f, ResetHeight = 32.f;
constexpr std::array<EnemyKind, 4> Kinds{{EnemyKind::Wanderer, EnemyKind::Chaser,
                                       EnemyKind::Armored, EnemyKind::Swift}};
inline float trackY(int row) { return FirstTrack + RowHeight * row; }
inline int hitSlider(CanvasPoint point) {
    if(point.x < Left-12 || point.x > Right+12)return -1;
    for(int row=0;row<4;++row)if(std::abs(point.y-trackY(row))<=12)return row;
    return -1;
}
inline float speedAt(float x) {
    return MinEnemySpeed + std::clamp((x-Left)/(Right-Left),0.f,1.f)*(MaxEnemySpeed-MinEnemySpeed);
}
inline float thumbX(float speed) {
    return Left + (speed-MinEnemySpeed)/(MaxEnemySpeed-MinEnemySpeed)*(Right-Left);
}
inline bool hitReset(CanvasPoint point) {
    return point.x>=ResetX&&point.x<=ResetX+ResetWidth&&point.y>=ResetY&&point.y<=ResetY+ResetHeight;
}
} // namespace bomber::speedControls
