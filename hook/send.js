#!/usr/bin/env node
'use strict';

// Detached sender: POST a snapshot to the board. Silent on every failure.

const dns = require('node:dns');
const path = require('node:path');
const { resolveHostConfig } = require('./lib/state');
const store = require('./lib/store');

const TIMEOUT_MS = 1500;

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
    const { address } = await dns.promises.lookup(host, { family: 4 });
    store.writeJsonAtomic(cacheFile, { host, ip: address });
    target = address;
  }
  await post(target, body);
}

main().catch(() => {});
