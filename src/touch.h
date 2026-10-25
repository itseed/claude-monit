#pragma once

// Only "is the screen pressed" is available on this board: the XPT2046 pen
// interrupt works but its SPI position data never changed (Task 7b ledger:
// library, bit-bang, display bus and a 60-combination pin scan all failed).
void touchBegin();
bool touchPressed();
