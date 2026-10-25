#include "weather_task.h"
#include <HTTPClient.h>
#include <WiFi.h>

namespace {
constexpr const char* URL =
    "http://api.open-meteo.com/v1/forecast?latitude=13.7563&longitude=100.5018"
    "&current=temperature_2m,weather_code&timezone=Asia%2FBangkok";
constexpr uint32_t REFRESH_MS = 15UL * 60 * 1000;
constexpr uint32_t RETRY_MS = 60UL * 1000;
constexpr uint32_t STALE_MS = 60UL * 60 * 1000;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Weather latest{};
uint32_t fetchedAt = 0;

bool fetchOnce() {
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(URL)) return false;
  bool ok = false;
  if (http.GET() == 200) {
    const String body = http.getString();
    Weather w{};
    if (parseWeather(body.c_str(), body.length(), w)) {
      portENTER_CRITICAL(&mux);
      latest = w;
      fetchedAt = millis();
      portEXIT_CRITICAL(&mux);
      ok = true;
    }
  }
  http.end();
  return ok;
}

void task(void*) {
  for (;;) {
    const bool ok = WiFi.status() == WL_CONNECTED && fetchOnce();
    vTaskDelay(pdMS_TO_TICKS(ok ? REFRESH_MS : RETRY_MS));
  }
}
}  // namespace

void weatherBegin() { xTaskCreatePinnedToCore(task, "weather", 8192, nullptr, 1, nullptr, 0); }

Weather weatherNow() {
  portENTER_CRITICAL(&mux);
  Weather w = latest;
  const uint32_t at = fetchedAt;
  portEXIT_CRITICAL(&mux);
  if (w.valid && millis() - at > STALE_MS) w.valid = false;
  return w;
}
