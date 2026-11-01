const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { fromStatusline, sameLimits } = require('../lib/usage');
const { record, recordUsage } = require('../lib/record');
const { buildSnapshot } = require('../lib/state');
const store = require('../lib/store');

const TAP = path.join(__dirname, '..', 'statusline-tap.js');

// Shape captured from a real Claude Code status line payload (seven_day absent).
const REAL = {
  session_id: 's1',
  context_window: { total_input_tokens: 112101, context_window_size: 1000000, used_percentage: 11 },
  rate_limits: { five_hour: { used_percentage: 13, resets_at: 1793557200 } },
};

function tmpDir() {
  return path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cm-usage-')), 'sessions');
}

test('fromStatusline reads context and the limits that are present', () => {
  assert.deepEqual(fromStatusline(REAL), { sessionId: 's1', ctx: 11, limits: { h5: 13, h5_reset: 1793557200 } });
  const both = fromStatusline({
    session_id: 's2',
    context_window: { used_percentage: 45.6 },
    rate_limits: { five_hour: { used_percentage: 99.7 }, seven_day: { used_percentage: 18, resets_at: 1794000000 } },
  });
  assert.deepEqual(both, { sessionId: 's2', ctx: 46, limits: { h5: 100, d7: 18, d7_reset: 1794000000 } });
});

test('fromStatusline tolerates missing or junk fields', () => {
  assert.deepEqual(fromStatusline({}), { sessionId: null, ctx: null, limits: null });
  assert.deepEqual(fromStatusline(null), { sessionId: null, ctx: null, limits: null });
  const junk = fromStatusline({ session_id: 's', context_window: { used_percentage: null },
    rate_limits: { five_hour: { used_percentage: 'x' } } });
  assert.deepEqual(junk, { sessionId: 's', ctx: null, limits: null });
  assert.equal(fromStatusline({ context_window: { used_percentage: -5 } }).ctx, 0);
});

test('sameLimits compares by value', () => {
  assert.ok(sameLimits({ h5: 1 }, { h5: 1 }));
  assert.ok(!sameLimits({ h5: 1 }, { h5: 2 }));
  assert.ok(sameLimits(null, undefined));
});

test('buildSnapshot carries ctx per session and limits when known', () => {
  const snap = buildSnapshot([
    { session_id: 'a', base: 'api', state: 'working', created_at: 1, updated_at: 2, ctx: 45 },
    { session_id: 'b', base: 'web', state: 'idle', created_at: 2, updated_at: 3 },
  ], 9, { h5: 32, h5_reset: 1000 });
  assert.equal(snap.sessions[0].ctx, 45);
  assert.equal('ctx' in snap.sessions[1], false);
  assert.deepEqual(snap.limits, { h5: 32, h5_reset: 1000 });
  assert.equal('limits' in buildSnapshot([], 9, null), false);
});

test('recordUsage stores ctx only for known sessions and sends only on change', () => {
  const dir = tmpDir();
  let t = 100;
  const now = () => t++;
  // unknown session: ctx ignored, limits still stored -> snapshot
  let snap = recordUsage({ sessionId: 's1', ctx: 11, limits: { h5: 13 } }, { dir, now });
  assert.deepEqual(snap.limits, { h5: 13 });
  assert.equal(snap.sessions.length, 0);
  // nothing changed -> no snapshot
  assert.equal(recordUsage({ sessionId: 's1', ctx: 11, limits: { h5: 13 } }, { dir, now }), null);

  record({ hook_event_name: 'UserPromptSubmit', session_id: 's1', cwd: '/x/api' }, { dir, home: '/h', now });
  snap = recordUsage({ sessionId: 's1', ctx: 11, limits: { h5: 13 } }, { dir, now });
  assert.equal(snap.sessions[0].ctx, 11);
  assert.equal(recordUsage({ sessionId: 's1', ctx: 11, limits: { h5: 13 } }, { dir, now }), null);
  assert.equal(recordUsage({ sessionId: 's1', ctx: 12, limits: { h5: 13 } }, { dir, now }).sessions[0].ctx, 12);

  // hook writes keep the ctx (it lives in its own file) ...
  snap = record({ hook_event_name: 'Stop', session_id: 's1', cwd: '/x/api' }, { dir, home: '/h', now });
  assert.equal(snap.sessions[0].ctx, 12);
  assert.equal(snap.sessions[0].state, 'done');
  // ... and SessionEnd removes it
  record({ hook_event_name: 'SessionEnd', session_id: 's1', cwd: '/x/api' }, { dir, home: '/h', now });
  assert.equal(store.readCtx(dir, 's1'), null);
});

test('statusline-tap writes usage files and exits quietly, even on bad input', () => {
  const home = fs.mkdtempSync(path.join(os.tmpdir(), 'cm-tap-'));
  const env = { ...process.env, CLAUDE_MONIT_HOME: home, CLAUDE_MONIT_HOST: '10.255.255.1' };
  const r = spawnSync(process.execPath, [TAP], { input: JSON.stringify(REAL), env, encoding: 'utf8' });
  assert.equal(r.status, 0);
  assert.equal(r.stdout + r.stderr, '');
  assert.deepEqual(JSON.parse(fs.readFileSync(path.join(home, 'usage.json'), 'utf8')), { h5: 13, h5_reset: 1793557200 });
  const bad = spawnSync(process.execPath, [TAP], { input: 'not json', env, encoding: 'utf8' });
  assert.equal(bad.status, 0);
  assert.equal(bad.stdout + bad.stderr, '');
});
