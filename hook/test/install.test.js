const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const INSTALL = path.join(__dirname, '..', 'install.js');
const EVENTS = ['SessionStart', 'UserPromptSubmit', 'PreToolUse', 'PermissionRequest', 'Notification',
  'PostToolUse', 'PostToolUseFailure', 'Stop', 'SessionEnd'];
const OTHER = { matcher: '', hooks: [{ type: 'command', command: 'node "/u/.pixel-agents/hooks/claude-hook.js"', timeout: 5 }] };

function settingsFile(content) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'cm-inst-'));
  const file = path.join(dir, 'settings.json');
  fs.writeFileSync(file, typeof content === 'string' ? content : JSON.stringify(content, null, 2));
  return file;
}

function run(file, ...args) {
  return spawnSync(process.execPath, [INSTALL, '--settings', file, ...args], { encoding: 'utf8' });
}

const ours = (group) => group.hooks.some((h) => h.command.includes('claude-monit-hook.js'));
const read = (file) => JSON.parse(fs.readFileSync(file, 'utf8'));

test('adds entries for all events, keeps existing hooks', () => {
  const file = settingsFile({ model: 'opus', hooks: { Notification: [OTHER] } });
  assert.equal(run(file).status, 0);
  const s = read(file);
  assert.equal(s.model, 'opus');
  assert.deepEqual(s.hooks.Notification[0], OTHER);
  for (const e of EVENTS) assert.equal(s.hooks[e].filter(ours).length, 1, e);
  const cmd = s.hooks.Stop.find(ours).hooks[0];
  assert.match(cmd.command, /^node ".*\/hook\/claude-monit-hook\.js"$/);
  assert.equal(cmd.timeout, 5);
});

test('idempotent', () => {
  const file = settingsFile({ hooks: { Notification: [OTHER] } });
  run(file);
  run(file);
  const s = read(file);
  for (const e of EVENTS) assert.equal(s.hooks[e].filter(ours).length, 1, e);
  assert.equal(s.hooks.Notification.length, 2);
});

test('uninstall removes only ours', () => {
  const file = settingsFile({ hooks: { Notification: [OTHER] } });
  run(file);
  assert.equal(run(file, '--uninstall').status, 0);
  const s = read(file);
  assert.deepEqual(s.hooks.Notification, [OTHER]);
  assert.equal(s.hooks.Stop, undefined);
});

test('aborts on invalid JSON', () => {
  const file = settingsFile('{ "hooks": ');
  const before = fs.readFileSync(file);
  const r = run(file);
  assert.notEqual(r.status, 0);
  assert.deepEqual(fs.readFileSync(file), before);
});

test('creates backup', () => {
  const original = { hooks: { Notification: [OTHER] } };
  const file = settingsFile(original);
  run(file);
  assert.deepEqual(read(`${file}.claude-monit.bak`), original);
});

test('works when settings file does not exist yet', () => {
  const file = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cm-inst-')), 'settings.json');
  assert.equal(run(file).status, 0);
  assert.equal(read(file).hooks.Stop.filter(ours).length, 1);
});
