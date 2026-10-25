#include "weather.h"
#include <ArduinoJson.h>
#include <math.h>

int wifiBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

WeatherIcon weatherIconFor(int c) {
  if (c == 0) return WeatherIcon::Sun;
  if (c == 1 || c == 2) return WeatherIcon::PartlyCloudy;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return WeatherIcon::Rain;
  if (c >= 95 && c <= 99) return WeatherIcon::Storm;
  return WeatherIcon::Cloud;  // overcast, fog, snow and unknown codes
}

bool parseWeather(const char* json, size_t len, Weather& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  JsonVariantConst cur = doc["current"];
  if (!cur["temperature_2m"].is<float>() || !cur["weather_code"].is<int>()) return false;
  out.tempC = (int)lroundf(cur["temperature_2m"].as<float>());
  out.code = cur["weather_code"].as<int>();
  out.valid = true;
  return true;
}
