#!/usr/bin/env python3
"""Capture README screenshots straight from the board's frame buffer.

Posts staged snapshots to the board, grabs GET /screen.bmp and writes 2x
nearest-neighbour PNGs/GIFs to docs/images/. Live sessions come back on the
next Claude Code hook event.

    python3 tools/screenshots.py [board-host]
"""
import io
import json
import sys
import time
import urllib.request
from pathlib import Path

from PIL import Image

HOST = sys.argv[1] if len(sys.argv) > 1 else "claude-monit.local"
OUT = Path(__file__).resolve().parent.parent / "docs" / "images"
MIN = 60 * 1000


def now_ms():
    return int(time.time() * 1000)


def post(sessions):
    now = now_ms()
    body = {"now": now, "sessions": [
        {"id": f"shot-{i}", "name": name, "state": state, "updated_at": now - age,
         "cwd": f"~/project/{name.split('#')[0]}", "tool": "Bash" if state == "waiting" else ""}
        for i, (name, state, age) in enumerate(sessions)]}
    req = urllib.request.Request(f"http://{HOST}/snapshot", data=json.dumps(body).encode(),
                                 headers={"content-type": "application/json"}, method="POST")
    urllib.request.urlopen(req, timeout=5).read()


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

    # 1. a fresh permission request: "!" bubble (shown for 3 s)
    post([("api", "waiting", 0), ("web", "done", 4 * MIN), ("mobile", "working", 0)])
    time.sleep(0.3)
    save_png("sessions-3", grab())

    # 2. six sessions, one waiting long enough to sweat
    six = [("api", "waiting", 20 * MIN), ("web", "done", 2 * MIN), ("mobile", "waiting", 6 * MIN),
           ("docs", "working", 0), ("claude-monit", "idle", 10 * MIN), ("agent-teams#2", "working", 0)]
    post(six)
    time.sleep(3.5)  # let the "!" bubbles pass
    frames = [grab() for _ in range(6)]
    save_png("sessions-6", frames[0])
    save_gif("sessions-6", frames, 400)

    # 3. more than six: page dots in the status bar
    post(six + [("fuse-web", "idle", 12 * MIN), ("pms-web", "working", 0)])
    time.sleep(3.5)
    save_png("sessions-8-pages", grab())

    # 4. nothing open: desk clock with Clawd strolling (one 40 s loop)
    post([])
    frames = []
    end = time.time() + 40
    while time.time() < end:
        frames.append(grab())
    save_png("clock", frames[0])
    save_gif("clock", frames, 500)


if __name__ == "__main__":
    main()
