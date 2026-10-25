#include "session_store.h"
#include <string.h>

static int priority(State s) {
  switch (s) {
    case State::Waiting: return 3;
    case State::Done: return 2;
    case State::Working: return 1;
    default: return 0;
  }
}

static uint32_t ageMs(const Session& s, uint32_t nowMs) {
  return s.ageAtReceiptMs + (uint32_t)(nowMs - s.receivedAtMs);
}

const SessionStore::Ack* SessionStore::findAck(const char* id) const {
  for (int i = 0; i < ackCount_; i++)
    if (!strcmp(acks_[i].id, id)) return &acks_[i];
  return nullptr;
}

bool SessionStore::applySnapshot(const Snapshot& s, uint32_t nowMs) {
  if (hasSnapshot_ && s.now < lastNow_ && lastNow_ - s.now < STALE_WINDOW_MS) return false;
  hasSnapshot_ = true;
  lastNow_ = s.now;

  Session next[MAX_SESSIONS];
  int n = 0;
  for (int i = 0; i < s.count && n < MAX_SESSIONS; i++) {
    const SnapshotEntry& e = s.entries[i];
    Session& d = next[n++];
    memcpy(d.id, e.id, sizeof(d.id));
    memcpy(d.name, e.name, sizeof(d.name));
    memcpy(d.cwd, e.cwd, sizeof(d.cwd));
    memcpy(d.tool, e.tool, sizeof(d.tool));
    d.state = d.reported = e.state;
    d.updatedAt = e.updatedAt;
    uint64_t age = s.now > e.updatedAt ? s.now - e.updatedAt : 0;
    d.ageAtReceiptMs = age > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)age;
    d.receivedAtMs = nowMs;

    const Ack* ack = findAck(d.id);
    if (ack && d.updatedAt <= ack->updatedAt && (d.state == State::Waiting || d.state == State::Done))
      d.state = State::Idle;

    const Session* prev = nullptr;
    for (int j = 0; j < count_; j++)
      if (!strcmp(sessions_[j].id, d.id)) prev = &sessions_[j];
    d.enteredAtMs = (prev && prev->state == d.state) ? prev->enteredAtMs : nowMs;
    if (d.state == State::Done && !(prev && prev->state == State::Done)) doneGen_++;
  }

  // Drop acks of sessions that left the snapshot.
  int k = 0;
  for (int i = 0; i < ackCount_; i++) {
    bool present = false;
    for (int j = 0; j < n; j++)
      if (!strcmp(next[j].id, acks_[i].id)) present = true;
    if (present) acks_[k++] = acks_[i];
  }
  ackCount_ = k;

  memcpy(sessions_, next, sizeof(Session) * n);
  count_ = n;
  return true;
}

bool SessionStore::acknowledge(const char* id) {
  for (int i = 0; i < count_; i++) {
    Session& s = sessions_[i];
    if (strcmp(s.id, id)) continue;
    if (s.state != State::Waiting && s.state != State::Done) return false;
    s.state = State::Idle;
    Ack* ack = const_cast<Ack*>(findAck(id));
    if (!ack && ackCount_ < MAX_SESSIONS) ack = &acks_[ackCount_++];
    if (ack) {
      memcpy(ack->id, s.id, sizeof(ack->id));
      ack->updatedAt = s.updatedAt;
    }
    return true;
  }
  return false;
}

void SessionStore::expire(uint32_t nowMs) {
  int k = 0;
  for (int i = 0; i < count_; i++) {
    const Session& s = sessions_[i];
    bool alert = s.reported == State::Waiting || s.reported == State::Done;
    if (ageMs(s, nowMs) <= (alert ? ALERT_TTL_MS : ACTIVE_TTL_MS)) sessions_[k++] = s;
  }
  count_ = k;
}

int SessionStore::rankByUrgency(int* idx) const {
  auto better = [&](int a, int b) {
    int pa = priority(sessions_[a].state), pb = priority(sessions_[b].state);
    if (pa != pb) return pa > pb;
    if (sessions_[a].updatedAt != sessions_[b].updatedAt) return sessions_[a].updatedAt > sessions_[b].updatedAt;
    return a < b;
  };
  for (int i = 0; i < count_; i++) idx[i] = i;
  for (int i = 1; i < count_; i++) {
    int v = idx[i], j = i - 1;
    while (j >= 0 && better(v, idx[j])) {
      idx[j + 1] = idx[j];
      j--;
    }
    idx[j + 1] = v;
  }
  return count_;
}

int SessionStore::page(int index, const Session** out, int per) const {
  if (count_ <= per) {
    if (index != 0) return 0;
    for (int i = 0; i < count_; i++) out[i] = &sessions_[i];
    return count_;
  }
  int idx[MAX_SESSIONS];
  rankByUrgency(idx);
  bool chosen[MAX_SESSIONS] = {};
  for (int r = index * per; r < count_ && r < (index + 1) * per; r++) chosen[idx[r]] = true;
  int n = 0;
  for (int i = 0; i < count_; i++)
    if (chosen[i]) out[n++] = &sessions_[i];
  return n;
}

int SessionStore::acknowledgeAll() {
  int changed = 0;
  for (int i = 0; i < count_; i++)
    if (acknowledge(sessions_[i].id)) changed++;
  return changed;
}

const Session* SessionStore::nextForDetails(const char* currentId) const {
  if (count_ == 0) return nullptr;
  int idx[MAX_SESSIONS];
  rankByUrgency(idx);
  for (int i = 0; i < count_; i++)
    if (currentId && !strcmp(sessions_[idx[i]].id, currentId)) return &sessions_[idx[(i + 1) % count_]];
  return &sessions_[idx[0]];
}

LedMode SessionStore::ledMode() const {
  bool done = false;
  for (int i = 0; i < count_; i++) {
    if (sessions_[i].state == State::Waiting) return LedMode::Waiting;
    if (sessions_[i].state == State::Done) done = true;
  }
  return done ? LedMode::Done : LedMode::Off;
}
