#!/usr/bin/env node
'use strict';

// Claude Code hook: record this session's state, then hand a snapshot of all
// sessions to a detached sender so Claude Code never waits on the network.

const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const os = require('node:os');
const {
  eventToState, sanitizeName, assignName, prune, buildSnapshot, toolFromEvent, displayCwd,
} = require('./lib/state');
const store = require('./lib/store');

function main() {
  const evt = JSON.parse(fs.readFileSync(0, 'utf8'));
  const state = eventToState(evt);
  if (!state || !evt.session_id) return;

  const dir = store.sessionsDir();
  const now = Date.now();
  let sessions = store.readAll(dir);

  if (state === 'end') {
    store.remove(dir, evt.session_id);
    sessions = sessions.filter((s) => s.session_id !== evt.session_id);
  } else {
    let s = sessions.find((x) => x.session_id === evt.session_id);
    if (!s) {
      const base = sanitizeName(path.basename(evt.cwd || ''));
      s = { session_id: evt.session_id, base, name: assignName(base, sessions), created_at: now };
      sessions.push(s);
    }
    s.state = state;
    s.updated_at = now;
    s.cwd = displayCwd(evt.cwd, os.homedir());
    const tool = toolFromEvent(evt);
    if (tool) s.tool = tool;
    store.write(dir, s);
  }

  const { keep, drop } = prune(sessions, now);
  for (const id of drop) store.remove(dir, id);

  spawn(process.execPath, [path.join(__dirname, 'send.js'), JSON.stringify(buildSnapshot(keep, now))], {
    detached: true,
    stdio: 'ignore',
    env: process.env,
  }).unref();
}

try {
  main();
} catch {
  // never disturb Claude Code
}
process.exitCode = 0;
