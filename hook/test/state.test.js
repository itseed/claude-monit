const test = require('node:test');
const assert = require('node:assert/strict');
const {
  eventToState, sanitizeName, assignName, prune, buildSnapshot, resolveHostConfig,
  toolFromEvent, displayCwd,
} = require('../lib/state');

const MIN = 60 * 1000;
const HOUR = 60 * MIN;

test('eventToState maps lifecycle events', () => {
  const cases = {
    SessionStart: 'idle',
    UserPromptSubmit: 'working',
    PreToolUse: 'working',
    PostToolUse: 'working',
    PostToolUseFailure: 'working',
    PermissionRequest: 'waiting',
    Stop: 'done',
    SessionEnd: 'end',
    SubagentStop: null,
    SomethingNew: null,
  };
  for (const [name, want] of Object.entries(cases)) {
    assert.equal(eventToState({ hook_event_name: name }), want, name);
  }
});

test('eventToState handles Notification types', () => {
  assert.equal(eventToState({ hook_event_name: 'Notification', notification_type: 'permission_prompt' }), 'waiting');
  assert.equal(eventToState({ hook_event_name: 'Notification', notification_type: 'idle_prompt' }), null);
  assert.equal(eventToState({ hook_event_name: 'Notification', message: 'Claude needs your permission to use Bash' }), 'waiting');
  assert.equal(eventToState({ hook_event_name: 'Notification', message: 'Claude is waiting for your input' }), null);
});

test('sanitizeName keeps ASCII, collapses non-ASCII, truncates', () => {
  assert.equal(sanitizeName('claude-monit'), 'claude-monit');
  assert.equal(sanitizeName('โปรเจกต์ใหม่'), '?');
  assert.equal(sanitizeName('app-ทดสอบ-v2'), 'app-?-v2');
  assert.equal(sanitizeName('x'.repeat(30)), 'x'.repeat(20));
  assert.equal(sanitizeName(''), 'session');
  assert.equal(sanitizeName(undefined), 'session');
});

test('assignName picks the lowest free suffix', () => {
  assert.equal(assignName('api', []), 'api');
  assert.equal(assignName('api', [{ name: 'web' }]), 'api');
  assert.equal(assignName('api', [{ name: 'api' }]), 'api#2');
  assert.equal(assignName('api', [{ name: 'api' }, { name: 'api#3' }]), 'api#2');
  assert.equal(assignName('api', [{ name: 'api#2' }]), 'api');
});

test('prune applies per-state TTLs', () => {
  const now = 100 * HOUR;
  const s = (id, state, age) => ({ session_id: id, state, updated_at: now - age, created_at: 0 });
  const { keep, drop } = prune([
    s('w', 'working', 31 * MIN),
    s('i', 'idle', 29 * MIN),
    s('wt', 'waiting', 7 * HOUR),
    s('d', 'done', 8 * HOUR + MIN),
  ], now);
  assert.deepEqual(keep.map((x) => x.session_id), ['i', 'wt']);
  assert.deepEqual(drop.sort(), ['d', 'w']);
});

test('buildSnapshot orders by created_at and strips internals', () => {
  const snap = buildSnapshot([
    { session_id: 'b', name: 'web', base: 'web', state: 'done', created_at: 20, updated_at: 30 },
    { session_id: 'a', name: 'api', base: 'api', state: 'waiting', created_at: 10, updated_at: 40 },
  ], 99);
  assert.deepEqual(snap, {
    now: 99,
    sessions: [
      { id: 'a', name: 'api', state: 'waiting', updated_at: 40, cwd: '', tool: '' },
      { id: 'b', name: 'web', state: 'done', updated_at: 30, cwd: '', tool: '' },
    ],
  });
});

test('resolveHostConfig precedence and cache', () => {
  assert.deepEqual(resolveHostConfig({ env: {}, configJson: null, cacheJson: null }),
    { host: 'claude-monit.local', cachedIp: null });
  assert.deepEqual(resolveHostConfig({ env: {}, configJson: { host: '192.168.1.50' }, cacheJson: null }),
    { host: '192.168.1.50', cachedIp: null });
  assert.deepEqual(resolveHostConfig({ env: { CLAUDE_MONIT_HOST: '10.0.0.9' }, configJson: { host: 'x' }, cacheJson: null }),
    { host: '10.0.0.9', cachedIp: null });
  assert.deepEqual(resolveHostConfig({ env: {}, configJson: null, cacheJson: { host: 'claude-monit.local', ip: '192.168.1.7' } }),
    { host: 'claude-monit.local', cachedIp: '192.168.1.7' });
  assert.deepEqual(resolveHostConfig({ env: {}, configJson: null, cacheJson: { host: 'other.local', ip: '192.168.1.7' } }),
    { host: 'claude-monit.local', cachedIp: null });
  assert.deepEqual(resolveHostConfig({ env: {}, configJson: { host: '192.168.1.50' }, cacheJson: { host: '192.168.1.50', ip: '1.1.1.1' } }),
    { host: '192.168.1.50', cachedIp: null });
});

test('buildSnapshot carries cwd and tool', () => {
  const snap = buildSnapshot([
    { session_id: 'a', name: 'api', state: 'waiting', created_at: 1, updated_at: 2, cwd: '~/x/api', tool: 'Bash' },
  ], 3);
  assert.equal(snap.sessions[0].cwd, '~/x/api');
  assert.equal(snap.sessions[0].tool, 'Bash');
});

test('toolFromEvent', () => {
  assert.equal(toolFromEvent({ hook_event_name: 'PermissionRequest', tool_name: 'Bash' }), 'Bash');
  assert.equal(toolFromEvent({ hook_event_name: 'PreToolUse', tool_name: 'Edit' }), 'Edit');
  assert.equal(toolFromEvent({ hook_event_name: 'Notification', message: 'Claude needs your permission to use WebFetch' }), 'WebFetch');
  assert.equal(toolFromEvent({ hook_event_name: 'Notification', message: 'Claude is waiting for your input' }), undefined);
  assert.equal(toolFromEvent({ hook_event_name: 'Stop' }), undefined);
  assert.equal(toolFromEvent({ hook_event_name: 'PermissionRequest', tool_name: 'mcp__very_long_server_name__tool' }).length <= 20, true);
});

test('displayCwd shortens home, strips non-ASCII, keeps the tail', () => {
  assert.equal(displayCwd('/Users/k/project/claude-monit', '/Users/k'), '~/project/claude-monit');
  assert.equal(displayCwd('/opt/app', '/Users/k'), '/opt/app');
  assert.equal(displayCwd('/Users/k/งาน/app', '/Users/k'), '~/?/app');
  const long = displayCwd(`/Users/k/${'a'.repeat(30)}/${'b'.repeat(20)}`, '/Users/k');
  assert.equal(long.length, 40);
  assert.ok(long.startsWith('...'));
  assert.ok(long.endsWith('b'.repeat(20)));
  assert.equal(displayCwd(undefined, '/Users/k'), '');
});
