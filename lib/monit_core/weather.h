#pragma once
#include <stddef.h>

enum class WeatherIcon { Sun, PartlyCloudy, Cloud, Rain, Storm };

struct Weather {
  bool valid;
  int tempC;
  int code;  // WMO weather code
};

// 0..4 signal bars from RSSI in dBm.
int wifiBars(int rssi);
WeatherIcon weatherIconFor(int wmoCode);
// Parses Open-Meteo `current.temperature_2m` / `current.weather_code`.
bool parseWeather(const char* json, size_t len, Weather& out);
