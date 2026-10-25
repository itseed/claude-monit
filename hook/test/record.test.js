const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { record } = require('../lib/record');
const store = require('../lib/store');

test('snapshot stamp is taken after our own write and sees writes finished before it', () => {
  const dir = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cm-rec-')), 'sessions');
  const stamps = [100, 102];
  let calls = 0;
  const now = () => {
    calls++;
    if (calls === 2) {
      // our file must already be on disk when the snapshot clock is read
      assert.equal(store.readAll(dir).find((s) => s.session_id === 'me').state, 'done');
      // another session finishes its write just before our stamp
      store.write(dir, { session_id: 'other', base: 'web', state: 'done', created_at: 1, updated_at: 101 });
    }
    return stamps[calls - 1];
  };
  const snap = record({ hook_event_name: 'Stop', session_id: 'me', cwd: '/x/api' }, { dir, home: '/h', now });
  assert.equal(snap.now, 102);
  assert.deepEqual(snap.sessions.map((s) => [s.id, s.state]).sort(), [['me', 'done'], ['other', 'done']]);
  assert.equal(store.readAll(dir).find((s) => s.session_id === 'me').updated_at, 100);
});

test('record returns null for ignored events', () => {
  const dir = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cm-rec-')), 'sessions');
  assert.equal(record({ hook_event_name: 'SubagentStop', session_id: 'x' }, { dir, home: '/h', now: Date.now }), null);
});
