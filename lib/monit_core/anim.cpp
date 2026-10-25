#include "anim.h"

int urgencyLevel(uint32_t waitingMs) {
  if (waitingMs >= 15UL * 60 * 1000) return 2;
  if (waitingMs >= 5UL * 60 * 1000) return 1;
  return 0;
}

WalkPose walkPose(uint32_t nowMs, int xMin, int xMax) {
  const uint32_t t = nowMs % WALK_CYCLE_MS;
  const uint8_t legs = (nowMs / 250) % 2;
  const int span = xMax - xMin;
  if (t < WALK_MS) return {xMin + (int)((int64_t)span * t / WALK_MS), ClawdPose::Walk, true, legs};
  if (t < WALK_MS + LOOK_MS) return {xMax, ClawdPose::LookUp, true, 0};
  if (t < 2 * WALK_MS + LOOK_MS) {
    const uint32_t back = t - WALK_MS - LOOK_MS;
    return {xMax - (int)((int64_t)span * back / WALK_MS), ClawdPose::Walk, false, legs};
  }
  return {xMin, ClawdPose::Sleep, false, 0};
}
