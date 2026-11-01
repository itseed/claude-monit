#!/usr/bin/env node
'use strict';

// Status line tap: Claude Code passes context and plan-limit usage only to the
// status line command, so the status line script pipes its input here too.
// Stores this session's context % and the account's limits, and sends a
// snapshot to the board only when a shown number changed. Silent, never fails.

const fs = require('node:fs');
const { fromStatusline } = require('./lib/usage');
const { recordUsage } = require('./lib/record');
const { dispatch } = require('./lib/dispatch');
const store = require('./lib/store');

function main() {
  const input = JSON.parse(fs.readFileSync(0, 'utf8'));
  const snap = recordUsage(fromStatusline(input), { dir: store.sessionsDir(), now: Date.now });
  if (snap) dispatch(snap);
}

try {
  main();
} catch {
  // never disturb the status line
}
process.exitCode = 0;
