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

uint8_t clawdFrame(State st, uint32_t nowMs) { return (nowMs / framePeriodMs(st)) % 2; }

void drawClawd(TFT_eSprite& spr, int x, int y, int scale, State st, uint8_t frame, bool dim) {
  const uint16_t body = dim ? spr.color565(0x6b, 0x3b, 0x2b) : spr.color565(0xd9, 0x77, 0x57);
  const uint16_t eye = spr.color565(0x14, 0x14, 0x13);
  const Frame fr = frameFor(st, frame);
  const int top = y - fr.lift * scale;
  for (int i = 0; i < fr.n; i++) {
    const Px& p = fr.px[i];
    spr.fillRect(x + p.x * scale, top + p.y * scale, p.w * scale, p.h * scale, p.part == EYE ? eye : body);
  }
}
