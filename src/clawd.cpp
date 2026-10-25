#include "clawd.h"

namespace {

enum Part : uint8_t { BODY, EYE };
struct Px {
  int8_t x, y, w, h;
  Part part;
};

#define LEGS {3, 10, 1, 3, BODY}, {6, 10, 1, 3, BODY}, {9, 10, 1, 3, BODY}, {12, 10, 1, 3, BODY}

const Px WAITING_0[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 5, 2, 2, BODY}, {14, 0, 2, 6, BODY},
                        {4, 4, 2, 3, EYE}, {10, 4, 2, 3, EYE}};
const Px WAITING_1[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 5, 2, 2, BODY}, {14, 3, 2, 4, BODY},
                        {4, 4, 2, 3, EYE}, {10, 4, 2, 3, EYE}};
const Px DONE_0[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 1, 2, 5, BODY}, {14, 1, 2, 5, BODY},
                     {4, 5, 2, 1, EYE}, {10, 5, 2, 1, EYE}};
const Px WORKING_0[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 6, 2, 2, BODY}, {14, 6, 2, 2, BODY},
                        {5, 4, 2, 2, EYE}, {11, 4, 2, 2, EYE}};
const Px WORKING_1[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 6, 2, 2, BODY}, {14, 6, 2, 2, BODY},
                        {4, 4, 2, 2, EYE}, {10, 4, 2, 2, EYE}};
const Px WALK_R_0[] = {{3, 10, 1, 3, BODY}, {6, 10, 1, 3, BODY}, {9, 10, 1, 3, BODY}, {12, 10, 1, 3, BODY},
                      {2, 2, 12, 8, BODY}, {0, 5, 2, 2, BODY}, {14, 7, 2, 2, BODY},
                      {5, 4, 2, 2, EYE}, {11, 4, 2, 2, EYE}};
const Px WALK_R_1[] = {{2, 10, 1, 3, BODY}, {5, 10, 1, 3, BODY}, {10, 10, 1, 3, BODY}, {13, 10, 1, 3, BODY},
                      {2, 2, 12, 8, BODY}, {0, 7, 2, 2, BODY}, {14, 5, 2, 2, BODY},
                      {5, 4, 2, 2, EYE}, {11, 4, 2, 2, EYE}};
const Px WALK_L_0[] = {{3, 10, 1, 3, BODY}, {6, 10, 1, 3, BODY}, {9, 10, 1, 3, BODY}, {12, 10, 1, 3, BODY},
                      {2, 2, 12, 8, BODY}, {0, 7, 2, 2, BODY}, {14, 5, 2, 2, BODY},
                      {3, 4, 2, 2, EYE}, {9, 4, 2, 2, EYE}};
const Px WALK_L_1[] = {{2, 10, 1, 3, BODY}, {5, 10, 1, 3, BODY}, {10, 10, 1, 3, BODY}, {13, 10, 1, 3, BODY},
                      {2, 2, 12, 8, BODY}, {0, 5, 2, 2, BODY}, {14, 7, 2, 2, BODY},
                      {3, 4, 2, 2, EYE}, {9, 4, 2, 2, EYE}};
const Px LOOK_UP[] = {LEGS, {2, 2, 12, 8, BODY}, {0, 6, 2, 2, BODY}, {14, 6, 2, 2, BODY},
                      {4, 3, 2, 2, EYE}, {10, 3, 2, 2, EYE}};
const Px IDLE_0[] = {LEGS, {2, 4, 12, 6, BODY}, {0, 7, 2, 2, BODY}, {14, 7, 2, 2, BODY},
                     {4, 6, 2, 1, EYE}, {10, 6, 2, 1, EYE}};

struct Frame {
  const Px* px;
  int n;
  int8_t lift;
};

#define FRAME(arr, lift) {arr, (int)(sizeof(arr) / sizeof(arr[0])), lift}

Frame frameFor(State st, uint8_t f) {
  switch (st) {
    case State::Waiting: return f ? Frame FRAME(WAITING_1, 1) : Frame FRAME(WAITING_0, 0);
    case State::Done: return f ? Frame FRAME(DONE_0, 3) : Frame FRAME(DONE_0, 0);
    case State::Working: return f ? Frame FRAME(WORKING_1, 1) : Frame FRAME(WORKING_0, 0);
    default: return FRAME(IDLE_0, 0);
  }
}

uint32_t framePeriodMs(State st) {
  switch (st) {
    case State::Waiting: return 400;  // 2.5 fps
    case State::Done: return 250;     // 4 fps
    case State::Working: return 200;  // 5 fps
    default: return 1000;             // 1 fps
  }
}

}  // namespace

uint8_t clawdFrame(State st, uint32_t nowMs, bool urgent) {
  return (nowMs / (urgent ? 150 : framePeriodMs(st))) % 2;
}

static void drawFrame(TFT_eSprite& spr, int x, int y, int scale, const Frame& fr, bool dim) {
  const uint16_t body = dim ? spr.color565(0x6b, 0x3b, 0x2b) : spr.color565(0xdb, 0x49, 0x00);
  const uint16_t eye = spr.color565(0x14, 0x14, 0x13);
  const int top = y - fr.lift * scale;
  for (int i = 0; i < fr.n; i++) {
    const Px& p = fr.px[i];
    spr.fillRect(x + p.x * scale, top + p.y * scale, p.w * scale, p.h * scale, p.part == EYE ? eye : body);
  }
}

void drawClawd(TFT_eSprite& spr, int x, int y, int scale, State st, uint8_t frame, bool dim) {
  drawFrame(spr, x, y, scale, frameFor(st, frame), dim);
}

void drawClawdPose(TFT_eSprite& spr, int x, int y, int scale, ClawdPose pose, bool facingRight, uint8_t frame) {
  switch (pose) {
    case ClawdPose::Walk:
      if (facingRight) drawFrame(spr, x, y, scale, frame ? Frame FRAME(WALK_R_1, 1) : Frame FRAME(WALK_R_0, 0), false);
      else drawFrame(spr, x, y, scale, frame ? Frame FRAME(WALK_L_1, 1) : Frame FRAME(WALK_L_0, 0), false);
      break;
    case ClawdPose::LookUp: drawFrame(spr, x, y, scale, FRAME(LOOK_UP, 0), false); break;
    default: drawFrame(spr, x, y, scale, FRAME(IDLE_0, 0), false); break;
  }
}
