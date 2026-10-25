# claude-monit

A desk display for Claude Code on an ESP32-2432S028 ("Cheap Yellow Display"). Each Claude Code session gets its own animated pixel-art Clawd. The board shows which session is **waiting for permission** and which is **done**, and the RGB LED blinks to get your attention.

```
Claude Code sessions ──hooks──▶ hook/claude-monit-hook.js ──HTTP──▶ ESP32 (claude-monit.local)
                                   └─ ~/.claude-monit/sessions/*.json
```

## What the screen shows

| State | Slot | Clawd | LED |
|---|---|---|---|
| waiting for permission | orange background, `WAITING!` blinks | waves | orange, blinking |
| done | green `done` | jumps | green flash once a second |
| working | blue `working...` | bobs | off |
| idle | dimmed `zzz` | sleeps | off |

- **Status bar:** date, Bangkok weather (Open-Meteo, every 15 min), WiFi signal and time (NTP, UTC+7).
- **No sessions:** the board turns into a desk clock, with Clawd strolling across, looking up at the clock and napping.
- **Up to 6 slots per page.** With more sessions, pages advance every 8 s (15 s on page 1 while something waits), and dots in the status bar show the page. Waiting and done sessions are always on page 1.
- **Effects:** a `!` bubble pops up when a session starts waiting, and a pixel burst plays when one finishes. After 5 minutes of waiting, Clawd waves faster and the frame blinks; after 15 minutes it starts sweating.

### Touch

On this board the touch panel only reports *that* it is pressed, not *where*.

- **Tap:** acknowledges every waiting/done session at once (they go idle and the LED turns off). If a details page is open, a tap closes it.
- **Double tap:** goes to the next page.
- **Hold (about 0.6 s):** opens the details page for the most urgent session: how long it has been waiting, the tool it wants (e.g. `Bash`) and its folder. Hold again to step to the next session.

## Setup

### 1. Flash the firmware

```bash
brew install platformio
pio run -e cyd -t upload          # board on /dev/cu.usbserial-2110
pio run -e cyd_demo -t upload     # optional: demo mode that cycles fake sessions, no WiFi needed
```

### 2. Connect the board to WiFi

On first boot the board opens an access point called **`claude-monit-setup`**:

1. Join it from your phone.
2. Open `http://192.168.4.1` if the portal does not pop up.
3. Choose your WiFi network and enter its password.

The board remembers the network. To move it to a different network, power it up where the saved network is out of range and it will open the portal again.

### 3. Install the Claude Code hook

```bash
node hook/install.js              # adds entries to ~/.claude/settings.json
node hook/install.js --uninstall  # removes only claude-monit's entries
```

Notes:

- The installer keeps every existing hook.
- It saves a backup to `~/.claude/settings.json.claude-monit.bak` before writing.
- Running it twice does not add duplicate entries.

### 4. If `claude-monit.local` does not resolve

Some routers block mDNS. Point the hook at the IP shown on the board's clock screen:

```bash
echo '{"host":"192.168.1.50"}' > ~/.claude-monit/config.json
# or per shell: export CLAUDE_MONIT_HOST=192.168.1.50
```

## Debugging

```bash
curl http://claude-monit.local/status     # sessions, LED mode, press count
ls ~/.claude-monit/sessions/              # what the hook thinks each session is doing
```

The hook stays out of Claude Code's way:

- It exits right away and never prints anything.
- Sending happens in a detached background process.
- If the board is off, nothing slows down.
- After the board reboots, it catches up on the next hook event.

## Tests

```bash
pio test -e native        # board logic: sessions, expiry, layout, LED, clock, weather, gestures
(cd hook && node --test)  # hook: event mapping, naming, sender, installer
```

## Layout

| Path | What it holds |
|---|---|
| `lib/monit_core/` | Board logic with no hardware dependency, unit-tested on the Mac |
| `src/` | Display, Clawd sprites, WiFi/HTTP, LED, touch, weather |
| `hook/` | The Claude Code hook, the detached sender and the installer |
| `docs/superpowers/` | Design spec and implementation plan |
