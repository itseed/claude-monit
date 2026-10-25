#pragma once
#include <stddef.h>
#include "session_store.h"

// Parses {"now":ms,"sessions":[{"id","name","state","updated_at"}]}.
// Returns false on invalid JSON, missing fields or unknown state.
// Extra sessions beyond MAX_SESSIONS and over-long strings are truncated.
bool parseSnapshot(const char* json, size_t len, Snapshot& out);
