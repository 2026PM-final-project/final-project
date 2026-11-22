#pragma once
#include "game.hpp"

namespace bomber {
// [01] Shared movement sampling for the Win32 shell and fixed-step regression tests.
inline Direction resolveMovement(Direction& buffered, Direction pressed, Direction held,
                                 bool firstStep, const Actor& actor,
                                 const std::function<bool(Direction)>& canMove) {
    // A render-frame key edge must not be queued again by later physics ticks.
    if (firstStep && pressed != Direction::None) buffered = pressed;
    if (buffered != Direction::None && actor.centered() && canMove(buffered)) {
        held = buffered;
        buffered = Direction::None;
    }
    return held;
}
} // namespace bomber
