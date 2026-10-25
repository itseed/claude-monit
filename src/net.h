#pragma once
#include <Arduino.h>
#include "display.h"

// Connects to WiFi (setup portal "claude-monit-setup" if needed), starts
// mDNS "claude-monit", NTP (UTC+7) and the HTTP API.
void netBegin(Display& d);
void netLoop();
// Handler for POST /snapshot; return false to answer HTTP 400.
void netOnSnapshot(bool (*cb)(const char* body, size_t len));
// Body for GET /status.
void netOnStatus(String (*fn)());
String netLocalIp();
