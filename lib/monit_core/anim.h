#pragma once
#include <stdint.h>

// 0 = waiting < 5 min, 1 = 5..15 min (faster waving, blinking frame), 2 = 15+ min (sweat too).
int urgencyLevel(uint32_t waitingMs);

// Desk-clock screen: Clawd walks right, looks up at the clock, walks back, naps.
enum class ClawdPose : uint8_t { Walk, LookUp, Sleep };
struct WalkPose {
  int x;
  ClawdPose pose;
  bool facingRight;
  uint8_t frame;  // leg frame while walking
};
constexpr uint32_t WALK_MS = 12000;
constexpr uint32_t LOOK_MS = 8000;
constexpr uint32_t NAP_MS = 8000;
constexpr uint32_t WALK_CYCLE_MS = 2 * WALK_MS + LOOK_MS + NAP_MS;

WalkPose walkPose(uint32_t nowMs, int xMin, int xMax);
