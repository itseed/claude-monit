#!/usr/bin/env node
'use strict';

// Claude Code hook: record this session's state, then hand a snapshot of all
// sessions to a detached sender so Claude Code never waits on the network.

const fs = require('node:fs');
const os = require('node:os');
const { record } = require('./lib/record');
const { dispatch } = require('./lib/dispatch');
const store = require('./lib/store');

function main() {
  const evt = JSON.parse(fs.readFileSync(0, 'utf8'));
  const snap = record(evt, { dir: store.sessionsDir(), home: os.homedir(), now: Date.now });
  if (snap) dispatch(snap);
}

try {
  main();
} catch {
  // never disturb Claude Code
}
process.exitCode = 0;
