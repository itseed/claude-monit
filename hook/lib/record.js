'use strict';

// One hook event: update this session's file, then build a snapshot of all
// sessions. The snapshot clock is read only after our own write, so any hook
// with an earlier stamp has already written its file when we read the folder
// (the board drops snapshots older than the newest it has seen).

const path = require('node:path');
const { eventToState, sanitizeName, prune, buildSnapshot, toolFromEvent, displayCwd } = require('./state');
const store = require('./store');

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

  const stamp = now();
  const { keep, drop } = prune(store.readAll(dir), stamp);
  for (const id of drop) store.remove(dir, id);
  return buildSnapshot(keep, stamp);
}

module.exports = { record };
