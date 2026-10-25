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

// Auto-advancing pages when there are more sessions than slots.
constexpr uint32_t PAGE_MS = 8000;
constexpr uint32_t URGENT_PAGE_MS = 15000;  // first page (holds waiting/done) stays longer

class Pager {
 public:
  // Current page for `pages` pages; `urgent` = something needs attention.
  int update(uint32_t nowMs, int pages, bool urgent);
  void next(uint32_t nowMs, int pages);  // manual advance (double tap)

 private:
  int page_ = 0;
  int pages_ = 1;
  uint32_t since_ = 0;
  bool started_ = false;
};
