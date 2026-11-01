'use strict';

// Pure logic for context and plan-limit usage, read from Claude Code's
// status line input (hooks do not receive these numbers).

function pct(v) {
  const n = Number(v);
  if (v === null || v === undefined || v === '' || !Number.isFinite(n)) return undefined;
  return Math.min(100, Math.max(0, Math.round(n)));
}

function epochSec(v) {
  const n = Number(v);
  return v !== null && v !== undefined && Number.isFinite(n) && n > 0 ? Math.floor(n) : undefined;
}

/** { sessionId, ctx, limits } from a status line payload; missing parts are null. */
function fromStatusline(input) {
  const sessionId = input && typeof input.session_id === 'string' ? input.session_id : null;
  const ctx = pct(input && input.context_window && input.context_window.used_percentage);

  const rl = (input && input.rate_limits) || {};
  const limits = {};
  const h5 = pct(rl.five_hour && rl.five_hour.used_percentage);
  const d7 = pct(rl.seven_day && rl.seven_day.used_percentage);
  if (h5 !== undefined) {
    limits.h5 = h5;
    const r = epochSec(rl.five_hour.resets_at);
    if (r) limits.h5_reset = r;
  }
  if (d7 !== undefined) {
    limits.d7 = d7;
    const r = epochSec(rl.seven_day.resets_at);
    if (r) limits.d7_reset = r;
  }
  return {
    sessionId,
    ctx: ctx === undefined ? null : ctx,
    limits: Object.keys(limits).length ? limits : null,
  };
}

function sameLimits(a, b) {
  return JSON.stringify(a || null) === JSON.stringify(b || null);
}

module.exports = { fromStatusline, sameLimits };
