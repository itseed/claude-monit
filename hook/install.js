#!/usr/bin/env node
'use strict';

// Adds (or with --uninstall removes) the claude-monit hook in Claude Code's
// settings.json. Other hooks are never touched.

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { writeJsonAtomic } = require('./lib/store');

const EVENTS = ['SessionStart', 'UserPromptSubmit', 'PreToolUse', 'PermissionRequest', 'Notification',
  'PostToolUse', 'PostToolUseFailure', 'Stop', 'SessionEnd'];
const MARKER = 'claude-monit-hook.js';
const HOOK_PATH = path.join(__dirname, MARKER);

function parseArgs(argv) {
  const opts = { uninstall: false, settings: path.join(os.homedir(), '.claude', 'settings.json') };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--uninstall') opts.uninstall = true;
    else if (argv[i] === '--settings') opts.settings = argv[++i];
  }
  return opts;
}

const isOurs = (group) => Array.isArray(group && group.hooks)
  && group.hooks.some((h) => typeof h.command === 'string' && h.command.includes(MARKER));

function main() {
  const opts = parseArgs(process.argv.slice(2));
  let settings = {};
  if (fs.existsSync(opts.settings)) {
    const raw = fs.readFileSync(opts.settings, 'utf8');
    try {
      settings = JSON.parse(raw);
    } catch (e) {
      console.error(`claude-monit: cannot parse ${opts.settings}: ${e.message}. Nothing changed.`);
      process.exit(1);
    }
    fs.writeFileSync(`${opts.settings}.claude-monit.bak`, raw);
  }

  const hooks = settings.hooks || {};
  for (const event of Object.keys(hooks)) {
    if (!Array.isArray(hooks[event])) continue;
    hooks[event] = hooks[event].filter((g) => !isOurs(g));
    if (hooks[event].length === 0) delete hooks[event];
  }
  if (!opts.uninstall) {
    for (const event of EVENTS) {
      (hooks[event] = hooks[event] || []).push({
        matcher: '',
        hooks: [{ type: 'command', command: `node "${HOOK_PATH}"`, timeout: 5 }],
      });
    }
  }
  settings.hooks = hooks;

  writeJsonAtomic(opts.settings, settings, true);
  console.log(`claude-monit: ${opts.uninstall ? 'removed from' : 'installed in'} ${opts.settings}`);
}

main();
