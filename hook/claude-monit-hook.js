#!/usr/bin/env node
'use strict';

// Claude Code hook: record this session's state, then hand a snapshot of all
// sessions to a detached sender so Claude Code never waits on the network.

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { record } = require('./lib/record');
const store = require('./lib/store');

function main() {
  const evt = JSON.parse(fs.readFileSync(0, 'utf8'));
  const snap = record(evt, { dir: store.sessionsDir(), home: os.homedir(), now: Date.now });
  if (!snap) return;
  // While the board is unreachable, don't pile up senders; the next event
  // after the backoff carries the full state anyway.
  if (Date.now() < store.offlineUntil()) return;

  spawn(process.execPath, [path.join(__dirname, 'send.js'), JSON.stringify(snap)], {
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
