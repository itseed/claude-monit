#pragma once
#include "weather.h"

// Fetches Bangkok weather from Open-Meteo every 15 min on a background task.
void weatherBegin();
// Latest weather; invalid when never fetched or older than 1 hour.
Weather weatherNow();
