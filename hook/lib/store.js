'use strict';

// One JSON file per session under <home>/sessions, written atomically.

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

function homeDir() {
  return process.env.CLAUDE_MONIT_HOME || path.join(os.homedir(), '.claude-monit');
}

function sessionsDir(home = homeDir()) {
  return path.join(home, 'sessions');
}

function fileFor(dir, id) {
  return path.join(dir, `${String(id).replace(/[^A-Za-z0-9_-]/g, '_')}.json`);
}

function readAll(dir = sessionsDir()) {
  let names;
  try {
    names = fs.readdirSync(dir);
  } catch {
    return [];
  }
  const out = [];
  for (const n of names) {
    if (!n.endsWith('.json')) continue;
    try {
      out.push(JSON.parse(fs.readFileSync(path.join(dir, n), 'utf8')));
    } catch {
      // half-written or corrupt file: skip
    }
  }
  return out;
}

function writeJsonAtomic(file, obj, pretty = false) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, pretty ? `${JSON.stringify(obj, null, 2)}\n` : JSON.stringify(obj));
  fs.renameSync(tmp, file);
}

function write(dir, session) {
  writeJsonAtomic(fileFor(dir, session.session_id), session);
}

function remove(dir, id) {
  try {
    fs.unlinkSync(fileFor(dir, id));
  } catch {
    // already gone
  }
}

function readOne(dir, id) {
  return readJson(fileFor(dir, id));
}

// Board-offline backoff shared by the hook and the sender.
const OFFLINE_FILE = 'offline.json';

function offlineUntil(home = homeDir()) {
  const mark = readJson(path.join(home, OFFLINE_FILE));
  return (mark && mark.until) || 0;
}

function markOffline(home, until) {
  writeJsonAtomic(path.join(home, OFFLINE_FILE), { until });
}

function clearOffline(home) {
  try {
    fs.unlinkSync(path.join(home, OFFLINE_FILE));
  } catch {
    // not marked
  }
}

function readJson(file) {
  try {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch {
    return null;
  }
}

module.exports = {
  homeDir, sessionsDir, readAll, readOne, write, remove, readJson, writeJsonAtomic,
  offlineUntil, markOffline, clearOffline,
};
