const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');

const HOOK = path.join(__dirname, '..', 'claude-monit-hook.js');

function tmpHome() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'cm-hook-'));
}

function runHook(home, payload, host = '10.255.255.1') {
  const input = typeof payload === 'string' ? payload : JSON.stringify(payload);
  const start = process.hrtime.bigint();
  const r = spawnSync(process.execPath, [HOOK], {
    input,
    env: { ...process.env, CLAUDE_MONIT_HOME: home, CLAUDE_MONIT_HOST: host },
    encoding: 'utf8',
  });
  return { ...r, ms: Number(process.hrtime.bigint() - start) / 1e6 };
}

function readSession(home, id) {
  return JSON.parse(fs.readFileSync(path.join(home, 'sessions', `${id}.json`), 'utf8'));
}

test('writes session file on UserPromptSubmit', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'UserPromptSubmit', session_id: 's1', cwd: '/x/claude-monit' });
  const s = readSession(home, 's1');
  assert.equal(s.state, 'working');
  assert.equal(s.name, 'claude-monit');
});

test('state changes keep name and created_at', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'SessionStart', session_id: 's1', cwd: '/x/api' });
  const first = readSession(home, 's1');
  runHook(home, { hook_event_name: 'PermissionRequest', session_id: 's1', cwd: '/x/api' });
  const s = readSession(home, 's1');
  assert.equal(s.state, 'waiting');
  assert.equal(s.created_at, first.created_at);
  assert.equal(s.name, 'api');
});

test('ignored events do not create sessions', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'SubagentStop', session_id: 's1', cwd: '/x/api' });
  assert.equal(fs.existsSync(path.join(home, 'sessions', 's1.json')), false);
  runHook(home, { hook_event_name: 'Stop', session_id: 's1', cwd: '/x/api' });
  assert.equal(fs.existsSync(path.join(home, 'sessions', 's1.json')), true);
});

test('SessionEnd removes file', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'SessionStart', session_id: 's1', cwd: '/x/api' });
  assert.equal(fs.existsSync(path.join(home, 'sessions', 's1.json')), true);
  runHook(home, { hook_event_name: 'SessionEnd', session_id: 's1', cwd: '/x/api' });
  assert.equal(fs.existsSync(path.join(home, 'sessions', 's1.json')), false);
});

test('second session same cwd gets #2', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'SessionStart', session_id: 'a', cwd: '/x/api' });
  runHook(home, { hook_event_name: 'SessionStart', session_id: 'b', cwd: '/y/api' });
  assert.equal(readSession(home, 'b').name, 'api#2');
});

test('session ids cannot escape the sessions dir', () => {
  const home = tmpHome();
  runHook(home, { hook_event_name: 'SessionStart', session_id: '../../evil', cwd: '/x/api' });
  assert.equal(fs.readdirSync(path.join(home, 'sessions')).length, 1);
  assert.equal(fs.existsSync(path.join(home, 'evil.json')), false);
  assert.equal(fs.existsSync(path.join(path.dirname(home), 'evil.json')), false);
});

test('hook exits fast when board unreachable', () => {
  const home = tmpHome();
  const r = runHook(home, { hook_event_name: 'Stop', session_id: 's1', cwd: '/x/api' });
  assert.equal(r.status, 0);
  assert.equal(r.stdout, '');
  assert.equal(r.stderr, '');
  assert.ok(r.ms < 300, `took ${r.ms} ms`);
});

test('malformed stdin exits 0 silently', () => {
  const home = tmpHome();
  const r = runHook(home, 'not json{');
  assert.equal(r.status, 0);
  assert.equal(r.stdout, '');
  assert.equal(r.stderr, '');
});

test('sends snapshot of all sessions to the board', async () => {
  const home = tmpHome();
  let resolveBody;
  const got = new Promise((r) => { resolveBody = r; });
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      res.end('ok');
      resolveBody({ url: req.url, method: req.method, body });
    });
  });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const host = `127.0.0.1:${server.address().port}`;
  try {
    runHook(home, { hook_event_name: 'PermissionRequest', session_id: 's1', cwd: '/x/api' }, host);
    const req = await Promise.race([got, new Promise((_, rej) => setTimeout(() => rej(new Error('no POST')), 3000))]);
    assert.equal(req.method, 'POST');
    assert.equal(req.url, '/snapshot');
    const snap = JSON.parse(req.body);
    assert.equal(snap.sessions.length, 1);
    assert.deepEqual(snap.sessions[0].state, 'waiting');
  } finally {
    server.close();
  }
});
