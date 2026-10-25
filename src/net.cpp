// WebServer must come before TFT_eSPI (via net.h): TFT_eSPI defines FS_NO_GLOBALS,
// which hides the global `FS` that WebServer.h relies on.
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "net.h"

namespace {
constexpr const char* AP_NAME = "claude-monit-setup";
constexpr const char* HOSTNAME = "claude-monit";
constexpr size_t MAX_BODY = 8 * 1024;

WebServer server(80);
bool (*snapshotCb)(const char*, size_t) = nullptr;
String (*statusFn)() = nullptr;
void (*screenFn)(Print&) = nullptr;
uint32_t screenSize = 0;
uint32_t lastReconnect = 0;

void handleSnapshot() {
  const String& body = server.arg("plain");
  if (body.length() > MAX_BODY) return server.send(413, "text/plain", "too large");
  if (!snapshotCb || !snapshotCb(body.c_str(), body.length()))
    return server.send(400, "text/plain", "bad snapshot");
  server.send(200, "text/plain", "ok");
}
}  // namespace

void netOnSnapshot(bool (*cb)(const char*, size_t)) { snapshotCb = cb; }
void netOnStatus(String (*fn)()) { statusFn = fn; }
void netOnScreen(void (*fn)(Print&), uint32_t size) {
  screenFn = fn;
  screenSize = size;
}
String netLocalIp() { return WiFi.localIP().toString(); }

void netBegin(Display& d) {
  WiFi.setHostname(HOSTNAME);
  WiFiManager wm;
  wm.setAPCallback([&d](WiFiManager*) { d.renderSetup(AP_NAME); });
  wm.setConfigPortalTimeout(180);
  d.renderMessage("claude-monit", "connecting to WiFi...");
  if (!wm.autoConnect(AP_NAME)) {
    d.renderMessage("WiFi failed", "restarting...");
    delay(3000);
    ESP.restart();
  }

  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, [] { server.send(200, "text/plain", "claude-monit ok"); });
  server.on("/snapshot", HTTP_POST, handleSnapshot);
  server.on("/status", HTTP_GET, [] { server.send(200, "application/json", statusFn ? statusFn() : "{}"); });
  server.on("/screen.bmp", HTTP_GET, [] {
    if (!screenFn) return server.send(404, "text/plain", "no screen");
    server.setContentLength(screenSize);
    server.send(200, "image/bmp", "");
    WiFiClient client = server.client();
    screenFn(client);
  });
  server.begin();
}

void netLoop() {
  server.handleClient();
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnect > 10000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }
}
