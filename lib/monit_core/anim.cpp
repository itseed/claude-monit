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

int Pager::update(uint32_t nowMs, int pages, bool urgent) {
  if (!started_) {
    started_ = true;
    since_ = nowMs;
  }
  if (pages != pages_ || page_ >= pages) {  // page set changed: back to page 1, fresh dwell
    pages_ = pages;
    page_ = 0;
    since_ = nowMs;
  }
  if (pages <= 1) return 0;
  const uint32_t dwell = (page_ == 0 && urgent) ? URGENT_PAGE_MS : PAGE_MS;
  if (nowMs - since_ >= dwell) {
    page_ = (page_ + 1) % pages;
    since_ = nowMs;
  }
  return page_;
}

void Pager::next(uint32_t nowMs, int pages) {
  started_ = true;
  pages_ = pages;
  page_ = pages > 1 ? (page_ + 1) % pages : 0;
  since_ = nowMs;
}
