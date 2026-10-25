#pragma once
#include <TFT_eSPI.h>
#include "anim.h"
#include "session_store.h"

constexpr int CLAWD_W = 16;
constexpr int CLAWD_H = 14;
constexpr int CLAWD_MAX_LIFT = 3;  // highest frame offset, in sprite pixels

// Frame index for `st` at time `nowMs` (each pose has 2 frames).
uint8_t clawdFrame(State st, uint32_t nowMs, bool urgent = false);
// x,y = top-left of the 16x14*scale box; frames may lift up to CLAWD_MAX_LIFT*scale above it.
void drawClawd(TFT_eSprite& spr, int x, int y, int scale, State st, uint8_t frame, bool dim);
// Desk-clock poses (walk / look up at the clock / nap).
void drawClawdPose(TFT_eSprite& spr, int x, int y, int scale, ClawdPose pose, bool facingRight, uint8_t frame);
