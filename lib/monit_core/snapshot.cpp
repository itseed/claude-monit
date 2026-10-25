#include "snapshot.h"
#include <ArduinoJson.h>
#include <string.h>

static bool parseState(const char* s, State& out) {
  if (!s) return false;
  if (!strcmp(s, "idle")) out = State::Idle;
  else if (!strcmp(s, "working")) out = State::Working;
  else if (!strcmp(s, "waiting")) out = State::Waiting;
  else if (!strcmp(s, "done")) out = State::Done;
  else return false;
  return true;
}

static void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

bool parseSnapshot(const char* json, size_t len, Snapshot& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  if (!doc["now"].is<uint64_t>() || !doc["sessions"].is<JsonArrayConst>()) return false;

  Snapshot s{};
  s.now = doc["now"].as<uint64_t>();
  for (JsonObjectConst e : doc["sessions"].as<JsonArrayConst>()) {
    if (s.count >= MAX_SESSIONS) break;
    const char* id = e["id"];
    if (!id || !*id || !e["updated_at"].is<uint64_t>()) return false;
    SnapshotEntry& d = s.entries[s.count];
    if (!parseState(e["state"], d.state)) return false;
    copyStr(d.id, sizeof(d.id), id);
    copyStr(d.name, sizeof(d.name), e["name"] | id);
    copyStr(d.cwd, sizeof(d.cwd), e["cwd"] | "");
    copyStr(d.tool, sizeof(d.tool), e["tool"] | "");
    d.updatedAt = e["updated_at"].as<uint64_t>();
    s.count++;
  }
  out = s;
  return true;
}
