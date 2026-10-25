'use strict';

// Pure logic for the claude-monit hook: event → state, naming, TTLs, snapshot.

const MIN = 60 * 1000;
const ACTIVE_TTL_MS = 30 * MIN; // working / idle
const ALERT_TTL_MS = 8 * 60 * MIN; // waiting / done
const DEFAULT_HOST = 'claude-monit.local';
const MAX_NAME = 20;

const EVENT_STATES = {
  SessionStart: 'idle',
  UserPromptSubmit: 'working',
  PreToolUse: 'working',
  PostToolUse: 'working',
  PostToolUseFailure: 'working',
  PermissionRequest: 'waiting',
  Stop: 'done',
  SessionEnd: 'end',
};

/** Returns the new state for a hook payload, or null to leave it unchanged. */
function eventToState(evt) {
  const name = evt && evt.hook_event_name;
  if (name === 'Notification') {
    if (evt.notification_type) return evt.notification_type === 'permission_prompt' ? 'waiting' : null;
    return /permission/i.test(evt.message || '') ? 'waiting' : null;
  }
  return EVENT_STATES[name] || null;
}

/** Printable ASCII only (runs of anything else become one '?'), max 20 chars. */
function sanitizeName(str) {
  const clean = String(str || '').replace(/[^\x20-\x7e]+/g, '?').trim().slice(0, MAX_NAME);
  return clean || 'session';
}

/** `base`, or `base#N` with the lowest N not already used by a live session. */
function assignName(base, liveSessions) {
  const taken = new Set(liveSessions.map((s) => s.name));
  if (!taken.has(base)) return base;
  for (let n = 2; ; n++) {
    if (!taken.has(`${base}#${n}`)) return `${base}#${n}`;
  }
}

function ttlFor(state) {
  return state === 'waiting' || state === 'done' ? ALERT_TTL_MS : ACTIVE_TTL_MS;
}

function prune(sessions, nowMs) {
  const keep = [];
  const drop = [];
  for (const s of sessions) {
    if (nowMs - s.updated_at > ttlFor(s.state)) drop.push(s.session_id);
    else keep.push(s);
  }
  return { keep, drop };
}

function buildSnapshot(sessions, nowMs) {
  return {
    now: nowMs,
    sessions: [...sessions]
      .sort((a, b) => a.created_at - b.created_at)
      .map((s) => ({
        id: s.session_id,
        name: s.name,
        state: s.state,
        updated_at: s.updated_at,
        cwd: s.cwd || '',
        tool: s.tool || '',
      })),
  };
}

const MAX_TOOL = 20;
const MAX_CWD = 40;

/** Tool the session is about to use / asking permission for, if the event says. */
function toolFromEvent(evt) {
  const name = evt && evt.hook_event_name;
  let tool;
  if (name === 'PreToolUse' || name === 'PermissionRequest') tool = evt.tool_name;
  else if (name === 'Notification') {
    const m = /permission to use (\S+)/i.exec(evt.message || '');
    tool = m && m[1];
  }
  return tool ? String(tool).replace(/[^\x21-\x7e]+/g, '?').slice(0, MAX_TOOL) : undefined;
}

/** `~/…` path, ASCII only, at most 40 chars keeping the tail. */
function displayCwd(cwd, home) {
  if (!cwd) return '';
  let p = home && (cwd === home || cwd.startsWith(`${home}/`)) ? `~${cwd.slice(home.length)}` : cwd;
  p = p.replace(/[^\x20-\x7e]+/g, '?');
  return p.length > MAX_CWD ? `...${p.slice(p.length - (MAX_CWD - 3))}` : p;
}

/** Board address: env CLAUDE_MONIT_HOST > config.json host > claude-monit.local. */
function resolveHostConfig({ env, configJson, cacheJson }) {
  const host = (env && env.CLAUDE_MONIT_HOST) || (configJson && configJson.host) || DEFAULT_HOST;
  const cachedIp = host.endsWith('.local') && cacheJson && cacheJson.host === host && cacheJson.ip
    ? cacheJson.ip
    : null;
  return { host, cachedIp };
}

module.exports = {
  ACTIVE_TTL_MS,
  ALERT_TTL_MS,
  DEFAULT_HOST,
  eventToState,
  sanitizeName,
  assignName,
  prune,
  buildSnapshot,
  resolveHostConfig,
  toolFromEvent,
  displayCwd,
};
