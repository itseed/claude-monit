'use strict';

// Hand a snapshot to the detached sender so the caller never waits on the network.

const path = require('node:path');
const { spawn } = require('node:child_process');
const store = require('./store');

function dispatch(snap) {
  // While the board is unreachable, don't pile up senders; the next event
  // after the backoff carries the full state anyway.
  if (Date.now() < store.offlineUntil()) return;
  spawn(process.execPath, [path.join(__dirname, '..', 'send.js'), JSON.stringify(snap)], {
    detached: true,
    stdio: 'ignore',
    env: process.env,
  }).unref();
}

module.exports = { dispatch };
