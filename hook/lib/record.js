'use strict';

// One hook event (or status line update): update files on disk, then build a
// snapshot of all sessions. The snapshot clock is read only after our own
// write, so any writer with an earlier stamp has already written its file when
// we read the folder (the board drops snapshots older than the newest it has seen).

const path = require('node:path');
const { eventToState, sanitizeName, prune, buildSnapshot, toolFromEvent, displayCwd } = require('./state');
const { sameLimits } = require('./usage');
const store = require('./store');

function snapshotFromDisk(dir, stamp) {
  const { keep, drop } = prune(store.readAll(dir), stamp);
  for (const id of drop) store.remove(dir, id);
  for (const s of keep) {
    const c = store.readCtx(dir, s.session_id);
    if (c !== null) s.ctx = c;
  }
  return buildSnapshot(keep, stamp, store.readLimits(dir));
}

function record(evt, { dir, home, now }) {
  const state = eventToState(evt);
  if (!state || !evt.session_id) return null;

  if (state === 'end') {
    store.remove(dir, evt.session_id);
  } else {
    const t = now();
    const s = store.readOne(dir, evt.session_id) || {
      session_id: evt.session_id,
      base: sanitizeName(path.basename(evt.cwd || '')),
      created_at: t,
    };
    s.state = state;
    s.updated_at = t;
    s.cwd = displayCwd(evt.cwd, home);
    const tool = toolFromEvent(evt);
    if (tool) s.tool = tool;
    store.write(dir, s);
  }

  return snapshotFromDisk(dir, now());
}

/**
 * Status line numbers: context % for a session the hook already knows (kept
 * in its own file so it never races the hook's session writes) and the
 * account's plan limits. Returns a snapshot only when something shown changed.
 */
function recordUsage({ sessionId, ctx, limits }, { dir, now }) {
  let changed = false;
  if (sessionId && ctx !== null && store.readOne(dir, sessionId)
      && store.readCtx(dir, sessionId) !== ctx) {
    store.writeCtx(dir, sessionId, ctx);
    changed = true;
  }
  if (limits && !sameLimits(store.readLimits(dir), limits)) {
    store.writeLimits(dir, limits);
    changed = true;
  }
  return changed ? snapshotFromDisk(dir, now()) : null;
}

module.exports = { record, recordUsage };
