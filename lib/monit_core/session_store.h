#pragma once
#include <stdint.h>

enum class State : uint8_t { Idle, Working, Waiting, Done };
enum class LedMode : uint8_t { Off, Waiting, Done };

constexpr int MAX_SESSIONS = 16;
constexpr int MAX_VISIBLE = 6;
constexpr uint32_t ACTIVE_TTL_MS = 30UL * 60 * 1000;  // working/idle
constexpr uint32_t ALERT_TTL_MS = 8UL * 3600 * 1000;  // waiting/done

struct SnapshotEntry {
  char id[40];
  char name[24];
  State state;
  uint64_t updatedAt;
};

struct Snapshot {
  uint64_t now;
  int count;
  SnapshotEntry entries[MAX_SESSIONS];
};

struct Session {
  char id[40];
  char name[24];
  State state;
  uint64_t updatedAt;
  uint32_t ageAtReceiptMs;
  uint32_t receivedAtMs;
};

class SessionStore {
 public:
  // Replaces the whole session set. Returns false if the snapshot is older
  // than the last one applied (out-of-order delivery) and was ignored.
  bool applySnapshot(const Snapshot& s, uint32_t nowMs);
  // Touch acknowledge: Waiting/Done -> Idle until a newer event arrives.
  bool acknowledge(const char* id);
  void expire(uint32_t nowMs);
  // Up to `max` sessions in snapshot order; when there are more than `max`,
  // keeps the most important ones (waiting > done > working > idle, newest first).
  int visible(const Session** out, int max = MAX_VISIBLE) const;
  LedMode ledMode() const;
  uint32_t doneGeneration() const { return doneGen_; }
  int count() const { return count_; }

 private:
  struct Ack {
    char id[40];
    uint64_t updatedAt;
  };
  Session sessions_[MAX_SESSIONS] = {};
  int count_ = 0;
  Ack acks_[MAX_SESSIONS] = {};
  int ackCount_ = 0;
  uint64_t lastNow_ = 0;
  bool hasSnapshot_ = false;
  uint32_t doneGen_ = 0;

  const Ack* findAck(const char* id) const;
};
