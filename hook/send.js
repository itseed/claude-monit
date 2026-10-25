#!/usr/bin/env node
'use strict';

// Detached sender: POST a snapshot to the board. Silent on every failure.

const dns = require('node:dns');
const path = require('node:path');
const { resolveHostConfig } = require('./lib/state');
const store = require('./lib/store');

const TIMEOUT_MS = 1500;
const OFFLINE_BACKOFF_MS = 30 * 1000;
const HARD_EXIT_MS = 5000;

async function post(host, body) {
  const res = await fetch(`http://${host}/snapshot`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body,
    signal: AbortSignal.timeout(TIMEOUT_MS),
  });
  return res.ok;
}

async function main() {
  const body = process.argv[2];
  if (!body) return;
  const home = store.homeDir();
  const cacheFile = path.join(home, 'host-cache.json');
  const { host, cachedIp } = resolveHostConfig({
    env: process.env,
    configJson: store.readJson(path.join(home, 'config.json')),
    cacheJson: store.readJson(cacheFile),
  });

  if (cachedIp) {
    try {
      if (await post(cachedIp, body)) return;
    } catch {
      // stale cache: resolve again below
    }
  }
  let target = host;
  if (host.endsWith('.local')) {
    const { address } = await Promise.race([
      dns.promises.lookup(host, { family: 4 }),
      new Promise((_, reject) => setTimeout(() => reject(new Error('dns timeout')), TIMEOUT_MS).unref()),
    ]);
    store.writeJsonAtomic(cacheFile, { host, ip: address });
    target = address;
  }
  if (!(await post(target, body))) throw new Error('board rejected snapshot');
}

// dns.lookup cannot be aborted, so never let a sender outlive HARD_EXIT_MS.
setTimeout(() => process.exit(0), HARD_EXIT_MS).unref();

main()
  .then(() => store.clearOffline(store.homeDir()))
  .catch(() => store.markOffline(store.homeDir(), Date.now() + OFFLINE_BACKOFF_MS))
  .finally(() => process.exit(0));
