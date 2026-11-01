#!/usr/bin/env python3
"""Capture README screenshots straight from the board's frame buffer.

Posts staged snapshots (with context % and plan limits) to the board, grabs
GET /screen.bmp and writes 2x nearest-neighbour PNGs/GIFs to docs/images/.
While it runs, the hook's offline backoff is set so live sessions don't
overwrite the staged ones; they come back on the next hook event after it.
Needs the normal firmware (`pio run -e cyd -t upload`), not cyd_mock.

    python3 tools/screenshots.py [board-host]
"""
import io
import json
import sys
import time
import urllib.request
import os
from pathlib import Path

from PIL import Image

HOST = sys.argv[1] if len(sys.argv) > 1 else "claude-monit.local"
OUT = Path(__file__).resolve().parent.parent / "docs" / "images"
MIN = 60 * 1000
OFFLINE = Path(os.environ.get("CLAUDE_MONIT_HOME", Path.home() / ".claude-monit")) / "offline.json"

# Staged plan limits: 5-hour 32% (resets in 2h 15m), weekly 61% (resets in 3d 4h).
def limits():
    t = int(time.time())
    return {"h5": 32, "h5_reset": t + 2 * 3600 + 15 * 60, "d7": 61, "d7_reset": t + 3 * 86400 + 4 * 3600}


def now_ms():
    return int(time.time() * 1000)


def post(sessions):
    """sessions: (name, state, age_ms, ctx %)"""
    now = now_ms()
    body = {"now": now, "limits": limits(), "sessions": [
        {"id": f"shot-{i}", "name": name, "state": state, "updated_at": now - age, "ctx": ctx,
         "cwd": f"~/project/{name.split('#')[0]}", "tool": "Bash" if state == "waiting" else ""}
        for i, (name, state, age, ctx) in enumerate(sessions)]}
    req = urllib.request.Request(f"http://{HOST}/snapshot", data=json.dumps(body).encode(),
                                 headers={"content-type": "application/json"}, method="POST")
    urllib.request.urlopen(req, timeout=5).read()


def status():
    with urllib.request.urlopen(f"http://{HOST}/status", timeout=5) as r:
        return json.loads(r.read())


def wait_page(pages, want_last, timeout=25):
    """Wait until the board has `pages` pages and shows the first, or the last (plan usage) one."""
    end = time.time() + timeout
    while time.time() < end:
        st = status()
        if st.get("pages") == pages and st.get("page") == (pages - 1 if want_last else 0):
            return
        time.sleep(0.3)
    raise SystemExit("board never reached the wanted page")


def is_usage_page(im):
    """A 2x2 session page has a dark grid line down the middle (x=159); the usage page does not."""
    col = [im.getpixel((2 * 159, 2 * y)) for y in range(40, 220)]
    return sum(1 for p in col if p != (0, 0, 0) and max(p) < 80) < 60


def grab_usage_page(tries=8):
    # /status and /screen.bmp each take seconds while the board renders, and a
    # page only stays 8 s, so check the grabbed frame itself and retry.
    wait_page(2, want_last=True)
    for _ in range(tries):
        im = grab()
        if is_usage_page(im):
            return im
    raise SystemExit("could not catch the plan usage page")


def grab():
    with urllib.request.urlopen(f"http://{HOST}/screen.bmp", timeout=15) as r:
        im = Image.open(io.BytesIO(r.read())).convert("RGB")
    return im.resize((im.width * 2, im.height * 2), Image.NEAREST)


def save_png(name, im):
    im.save(OUT / f"{name}.png", optimize=True)
    print("wrote", name + ".png")


def save_gif(name, frames, ms):
    frames[0].save(OUT / f"{name}.gif", save_all=True, append_images=frames[1:], duration=ms, loop=0,
                   optimize=True)
    print("wrote", name + ".gif", len(frames), "frames")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    OFFLINE.parent.mkdir(parents=True, exist_ok=True)
    OFFLINE.write_text(json.dumps({"until": now_ms() + 10 * MIN}))  # hold live senders off
    try:
        shoot()
    finally:
        OFFLINE.unlink(missing_ok=True)


def shoot():
    # 1. a fresh permission request: "!" bubble (shown for 3 s)
    post([("api", "waiting", 0, 23), ("web", "done", 4 * MIN, 57), ("mobile", "working", 0, 8)])
    time.sleep(0.3)
    save_png("sessions-3", grab())

    # 2. six sessions, one waiting long enough to sweat
    six = [("api", "waiting", 20 * MIN, 41), ("web", "done", 2 * MIN, 64), ("mobile", "waiting", 6 * MIN, 12),
           ("docs", "working", 0, 27), ("claude-monit", "idle", 10 * MIN, 5), ("agent-teams#2", "working", 0, 86)]
    post(six)
    wait_page(2, want_last=False)  # one page of sessions + plan usage
    time.sleep(3.5)  # let the "!" bubbles pass
    frames = [grab() for _ in range(6)]
    save_png("sessions-6", frames[0])
    save_gif("sessions-6", frames, 400)

    # 3. more than six: page dots in the status bar
    post(six + [("fuse-web", "idle", 12 * MIN, 33), ("pms-web", "working", 0, 18)])
    wait_page(3, want_last=False)
    time.sleep(3.5)
    save_png("sessions-8-pages", grab())

    # 4. plan usage, the last page: limits with reset times and context per session
    post([("api", "working", 0, 41), ("web", "idle", 2 * MIN, 64), ("agent-teams#2", "working", 0, 86),
          ("claude-monit", "idle", 10 * MIN, 12)])
    save_png("usage", grab_usage_page())

    # 5. nothing open: desk clock with Clawd strolling (one 40 s loop)
    post([])
    frames = []
    end = time.time() + 40
    while time.time() < end:
        frames.append(grab())
    save_png("clock", frames[0])
    save_gif("clock", frames, 500)


if __name__ == "__main__":
    main()
