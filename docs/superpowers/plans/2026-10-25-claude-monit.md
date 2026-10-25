# claude-monit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** จอ ESP32 CYD ที่แสดง Clawd หนึ่งตัวต่อหนึ่ง session ของ Claude Code บน Mac พร้อมไฟ LED เตือนตอนรออนุญาตหรือทำงานเสร็จ

**Architecture:**
- ฝั่ง Mac ใช้สคริปต์ hook ภาษา Node ตัวเดียว ทำหน้าที่แปลง hook event เป็นสถานะ แล้วเก็บไว้ในไฟล์ `~/.claude-monit/sessions/<id>.json` หนึ่งไฟล์ต่อ session
- ทุกครั้งที่ hook ทำงาน สคริปต์จะ spawn process ลูกแบบ detached ให้ POST **snapshot ของทุก session** ไปที่บอร์ด แล้วจบตัวเองทันที
- เฟิร์มแวร์แบ่งเป็นสองส่วน
  - `lib/monit_core`: logic ล้วน (เก็บ session, หมดอายุ, รับทราบ, จัดช่อง, โหมด LED) ทดสอบบน Mac ด้วย PlatformIO native
  - `src/`: ส่วนที่ต้องคุยกับฮาร์ดแวร์ (จอ, ทัช, LED, WiFi/HTTP)

**Tech Stack:**
- PlatformIO + Arduino-ESP32
- TFT_eSPI, XPT2046_Touchscreen, WiFiManager (tzapu), ArduinoJson 7, ESPmDNS, WebServer
- Unity (native test)
- Node 22 (`node:test`, ไม่มี dependency ภายนอก)

**Spec:** `docs/superpowers/specs/2026-10-25-claude-monit-design.md`

## Global Constraints

- บอร์ด ESP32-2432S028R จอ ILI9341 320×240 แนวนอน, ทัช XPT2046 (SPI แยก), LED RGB active-low R=GPIO4 G=GPIO16 B=GPIO17, พอร์ตแฟลช `/dev/cu.usbserial-2110`
- ชื่อ mDNS `claude-monit.local`, ชื่อ AP ตั้งค่า `claude-monit-setup`
- hook ต้องจบเร็ว, exit 0 ทุกกรณี, ห้ามพิมพ์ลง stdout/stderr, connect timeout ของการส่งประมาณ 0.5 วินาที (ทำใน process ลูก)
- **ห้ามแก้หรือลบ hook เดิม** ใน `~/.claude/settings.json` ทำได้แค่เพิ่มหรือลบ entry ของตัวเอง (entry ที่ command มีคำว่า `claude-monit-hook.js`)
- สี: ตัวละคร `#db4900` (ส้มอิฐ — ผู้ใช้ขอหลังเห็นจอจริง; `#d97757` ออกมาเป็นสีโอรส), พื้นหลัง `#141413`, waiting พื้นช่อง `#3a2a12` ข้อความ `#e8a33d`, done `#5fb36b`, working `#6a9bd8`, idle `#666666`
- หมดอายุ: working/idle เงียบเกิน 30 นาที, waiting/done เงียบเกิน 8 ชั่วโมง; `SessionEnd` ลบทันที
- ลำดับความสำคัญ: waiting > done > working > idle; แสดงได้สูงสุด 6 ช่อง
- ตัวละครขนาด 16×14 พิกเซลต่อเฟรม ท่าละ 2 เฟรม ขยายแบบจำนวนเต็ม หน้าตาตาม mockup `character.html` แบบ A
- ข้อความบนจอใช้ ASCII เท่านั้น (ไม่รองรับภาษาไทยบนจอ)

## Review Focus

1. **บอร์ดออฟไลน์หรือ resolve ชื่อไม่ได้:** hook ต้องจบภายใน 300 ms ไม่ทำให้ Claude Code ช้า → test ใน Task 5 (`hook exits fast when board unreachable`)
2. **POST body เสียหรือมี session มากเกินไป:** บอร์ดต้องตอบ 400 และคงสถานะเดิมไว้ หรือตัดให้เหลือ `MAX_SESSIONS` → test ใน Task 2 (`test_parse_rejects_garbage`, `test_parse_truncates`)
3. **snapshot มาไม่เรียงลำดับ:** hook สองตัวยิงเกือบพร้อมกัน แล้วตัวเก่ามาถึงทีหลัง บอร์ดต้องไม่ถอยกลับไปใช้สถานะเก่า → test ใน Task 2 (`test_older_snapshot_ignored`)
4. **`settings.json` มี hook ของคนอื่นอยู่หรือ JSON เสีย:** install ต้องไม่ทำลายของเดิม ถ้า parse ไม่ได้ให้ abort และต้องติดตั้งซ้ำได้โดยไม่ซ้อน → test ใน Task 5
5. **ชื่อโฟลเดอร์มีภาษาไทย อักขระพิเศษ หรือยาวมาก:** ชื่อต้องเป็น ASCII ที่อ่านได้ และไม่ล้นช่อง → test ใน Task 4 (`sanitizeName`) และตัดตามความกว้างพิกเซลใน Task 6

---

## File Structure

```
platformio.ini                  env cyd (esp32dev) + env native (test)
lib/monit_core/
  session_store.h/.cpp          Session, SessionStore: applySnapshot/acknowledge/expire/visible/ledMode
  snapshot.h/.cpp               parseSnapshot(json) -> Snapshot (ArduinoJson)
  layout.h/.cpp                 computeLayout(count) -> Layout
  led_pattern.h/.cpp            ledColorAt(mode, msInMode) -> Rgb
test/test_core/test_main.cpp    Unity tests ของ monit_core
src/
  main.cpp                      setup/loop ที่ประกอบทุกส่วนเข้าด้วยกัน
  hw_pins.h                     ขาของทัชและ LED
  clawd.h/.cpp                  เฟรม pixel art + drawClawd()
  display.h/.cpp                render ทั้งจอลง sprite 8-bit
  touch.h/.cpp                  อ่านทัช แล้ว map เป็นพิกัดจอ
  led.h/.cpp                    ขับ LED RGB ด้วย PWM
  net.h/.cpp                    WiFiManager, mDNS, HTTP server
hook/
  lib/state.js                  pure: eventToState, sanitizeName, assignName, prune, buildSnapshot, resolveHostConfig
  lib/store.js                  อ่าน/เขียนไฟล์ session แบบ atomic
  claude-monit-hook.js          entry ที่ Claude Code เรียก
  send.js                       process ลูก: resolve host แล้ว POST
  install.js                    install / uninstall ใน settings.json
  test/*.test.js
```

---

### Task 1: Scaffold, toolchain และทดสอบฮาร์ดแวร์

**Files:**
- Create: `platformio.ini`, `src/main.cpp` (ชั่วคราวสำหรับ smoke test), `src/hw_pins.h`, `.gitignore`

**Interfaces:**
- Produces:
  - env `cyd` ที่มี TFT_eSPI build_flags ตั้งค่าเสร็จแล้ว
  - env `native` ที่ build `lib/monit_core` ได้
  - `hw_pins.h` ที่มี `TOUCH_CS 33, TOUCH_IRQ 36, TOUCH_MOSI 32, TOUCH_MISO 39, TOUCH_CLK 25, LED_R 4, LED_G 16, LED_B 17`

- [ ] **Step 1:** `git init` แล้วสร้าง `.gitignore` ที่มี `.pio/`, `.superpowers/`, `node_modules/`, `.DS_Store`
- [ ] **Step 2:** ติดตั้ง PlatformIO ด้วย `brew install platformio` แล้วตรวจด้วย `pio --version` ต้องได้เวอร์ชัน 6.x ขึ้นไป
- [ ] **Step 3: เขียน `platformio.ini`**
  - `[env:cyd]`: `platform = espressif32`, `board = esp32dev`, `framework = arduino`, `monitor_speed = 115200`, `upload_port = /dev/cu.usbserial-2110`
  - lib_deps: `bodmer/TFT_eSPI@^2.5.43`, `paulstoffregen/XPT2046_Touchscreen`, `tzapu/WiFiManager@^2.0.17`, `bblanchon/ArduinoJson@^7`
  - build_flags: `-DUSER_SETUP_LOADED -DILI9341_2_DRIVER -DTFT_WIDTH=240 -DTFT_HEIGHT=320 -DUSE_HSPI_PORT -DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=-1 -DTFT_BL=21 -DTFT_BACKLIGHT_ON=HIGH -DLOAD_GLCD -DLOAD_FONT2 -DLOAD_FONT4 -DSMOOTH_FONT -DSPI_FREQUENCY=55000000 -DSPI_READ_FREQUENCY=20000000`
  - `[env:native]`: `platform = native`, `test_framework = unity`, `lib_deps = bblanchon/ArduinoJson@^7`, `build_src_filter = -<*>`
- [ ] **Step 4: เขียน smoke test ชั่วคราวใน `src/main.cpp`**
  - `tft.setRotation(1)` แล้ววาดแถบสีแดง/เขียว/น้ำเงิน/ขาว พร้อมข้อความ "TL" ที่มุมซ้ายบน
  - วาดสี่เหลี่ยมสี `#d97757` บนพื้น `#141413`
  - ใน loop: แตะจอแล้ว Serial print ค่า raw x/y/z, และให้ LED วนสี R→G→B ทุก 1 วินาที
- [ ] **Step 5:** รัน `pio run -e cyd -t upload && pio device monitor -e cyd` ต้อง upload สำเร็จ
- [ ] **Step 6: ตรวจด้วยตาบนบอร์ดจริง**
  - ข้อความ TL อยู่มุมซ้ายบน ไม่กลับด้าน
  - แถบสีถูกต้อง ไม่ inverted
  - ถ้าสีกลับด้าน ให้เพิ่ม `-DTFT_INVERSION_ON` ถ้าภาพเพี้ยน ให้ลอง `-DILI9341_DRIVER` แทน `ILI9341_2_DRIVER`
  - แตะมุมทั้ง 4 แล้วจดค่า raw min/max ของ x และ y ลงคอมเมนต์ใน `hw_pins.h` เป็น `TOUCH_X_MIN/MAX`, `TOUCH_Y_MIN/MAX` (ค่าที่มักเจอคือ x 200–3700, y 240–3800)
  - LED วนครบ 3 สี
- [ ] **Step 7: Commit:** `chore: scaffold platformio project and hardware smoke test`

### Task 2: monit_core — SessionStore และ snapshot

**Files:**
- Create: `lib/monit_core/session_store.h/.cpp`, `lib/monit_core/snapshot.h/.cpp`
- Test: `test/test_core/test_main.cpp`

**Interfaces:**
- Produces (ห้าม include `Arduino.h` ใน lib นี้):
```cpp
enum class State : uint8_t { Idle, Working, Waiting, Done };
enum class LedMode : uint8_t { Off, Waiting, Done };
constexpr int MAX_SESSIONS = 16;
constexpr int MAX_VISIBLE = 6;
constexpr uint32_t ACTIVE_TTL_MS = 30UL*60*1000;   // working/idle
constexpr uint32_t ALERT_TTL_MS  = 8UL*3600*1000;  // waiting/done
struct SnapshotEntry { char id[40]; char name[24]; State state; uint64_t updatedAt; };
struct Snapshot { uint64_t now; int count; SnapshotEntry entries[MAX_SESSIONS]; };
bool parseSnapshot(const char* json, size_t len, Snapshot& out); // false = invalid
struct Session { char id[40]; char name[24]; State state; uint64_t updatedAt;
                 uint32_t ageAtReceiptMs; uint32_t receivedAtMs; };
class SessionStore {
 public:
  bool applySnapshot(const Snapshot& s, uint32_t nowMs); // false = ignored (older)
  bool acknowledge(const char* id);                      // true if state changed to Idle
  void expire(uint32_t nowMs);
  int  visible(const Session** out, int max = MAX_VISIBLE) const;
  LedMode ledMode() const;
  uint32_t doneGeneration() const; // เพิ่มทุกครั้งที่มี session ใหม่เข้าสถานะ Done
  int count() const;
};
```
- JSON ที่รับ: `{"now":<epoch ms>,"sessions":[{"id":"…","name":"…","state":"waiting|done|working|idle","updated_at":<epoch ms>}]}` โดย sessions ถูกเรียงตามเวลาที่สร้างมาจากฝั่ง Mac แล้ว

- [ ] **Step 1: เขียน failing tests ใน `test/test_core/test_main.cpp`**
  - `test_parse_valid`: แปลง 2 entries ได้ครบ ทั้ง state, name, updatedAt, now
  - `test_parse_rejects_garbage`: กรณี `"not json"`, `{}`, ไม่มี `now`, หรือ state ไม่รู้จัก ต้องได้ `false`
  - `test_parse_truncates`: ถ้ามี 20 entries ต้องได้ `count == MAX_SESSIONS`, ถ้า name ยาว 60 ตัวต้องถูกตัดและปิดด้วย `\0`
  - `test_apply_replaces_set`: snapshot แรกมี A, B ส่วนตัวที่สองมีแค่ B ผลต้องเหลือ count 1 (session ที่ไม่อยู่ใน snapshot ล่าสุดถูกลบ)
  - `test_older_snapshot_ignored`: apply now=2000 แล้วตามด้วย now=1000 ต้องได้ `false` และสถานะไม่เปลี่ยน
  - `test_ack_sticks_until_newer_event`: A=done updatedAt=100 → `acknowledge("A")` ได้ Idle → snapshot ใหม่ที่ A=done updatedAt=100 ต้องยังเป็น Idle → snapshot ที่ A=waiting updatedAt=200 ต้องเป็น Waiting
  - `test_ack_only_done_or_waiting`: acknowledge session ที่ working ต้องได้ `false`
  - `test_expire_active_30min`: working ที่อายุ ณ ตอนรับ 0 ต้องยังอยู่ที่ nowMs+29 นาที และหายที่ +31 นาที
  - `test_expire_alert_8h`: waiting ต้องยังอยู่ที่ +7 ชั่วโมง และหายที่ +8 ชั่วโมง 1 นาที
  - `test_age_uses_snapshot_clock`: snapshot now=10,000,000, updatedAt = now − 29 นาที, state working → ที่ nowMs+2 นาที ต้องหายแล้ว
  - `test_visible_priority_over_6`: มี 8 sessions (1 waiting, 1 done, ที่เหลือ idle/working) visible ต้องได้ 6 ช่อง โดยมี waiting และ done อยู่ด้วย และ**เรียงตามลำดับเดิมใน snapshot**
  - `test_visible_keeps_order_under_6`: มี 3 sessions ต้องได้ลำดับเดิม
  - `test_led_mode`: ถ้ามี waiting ใดๆ ได้ `Waiting`; มีแค่ done ได้ `Done`; ไม่มีเลยได้ `Off`; session ที่ ack แล้วไม่นับ
  - `test_done_generation_increments`: idle→done ต้องเพิ่มขึ้น 1; done→done (snapshot ซ้ำ) ต้องไม่เพิ่ม
- [ ] **Step 2:** รัน `pio test -e native` ต้อง FAIL (compile error เพราะยังไม่มี symbol)
- [ ] **Step 3: Implement ตาม Interfaces**
  - อายุของ session = `ageAtReceiptMs + (nowMs - receivedAtMs)` โดย `ageAtReceiptMs = s.now - updatedAt` (บอร์ดไม่ต้องรู้เวลาจริง)
  - เก็บ ack เป็น `id + ackedUpdatedAt` แยกไว้ ตอน apply ถ้า `updatedAt <= ackedUpdatedAt` และสถานะเป็น Waiting/Done ให้บังคับเป็น Idle
  - ack ของ session ที่หายไปจาก snapshot ให้ลบทิ้งด้วย
  - ใช้ `uint32_t` กับ millis จึงต้องคิดแบบ wrap-safe คือใช้การลบแบบ unsigned
- [ ] **Step 4:** รัน `pio test -e native` ต้อง PASS ครบทุก test
- [ ] **Step 5: Commit:** `feat(core): session store with snapshot parsing, expiry and ack`

### Task 3: monit_core — layout และรูปแบบ LED

**Files:**
- Create: `lib/monit_core/layout.h/.cpp`, `lib/monit_core/led_pattern.h/.cpp`
- Test: เพิ่มใน `test/test_core/test_main.cpp`

**Interfaces:**
- Consumes: `LedMode` จาก Task 2
- Produces:
```cpp
struct Cell { int16_t x, y, w, h; };
struct Layout { int cols, rows, scale; int count; Cell cells[MAX_VISIBLE]; };
Layout computeLayout(int count);          // count 0..6
struct Rgb { uint8_t r, g, b; };
Rgb ledColorAt(LedMode mode, uint32_t msInMode);
```

- [ ] **Step 1: เขียน failing tests**
  - `test_layout_grid`: ตรวจ (cols,rows,scale) ตาม count — 0→(1,1,8) count 0; 1→(1,1,8); 2→(2,1,6); 3→(2,2,4); 4→(2,2,4); 5→(3,2,3); 6→(3,2,3)
  - `test_layout_cells_cover_screen`: ที่ count=6 ต้องได้ cells[0]={0,0,106,120}, cells[5].x+w เท่ากับ 320 (ช่องสุดท้ายในแถวกินพิกเซลที่เหลือ), cells[3].y==120
  - `test_led_waiting_blink`: ที่ 0 ms ได้ {255,60,0}, ที่ 250 ms ได้ {0,0,0}, ที่ 500 ms ได้ {255,60,0} (กะพริบ 2 Hz)
  - `test_led_done_then_steady`: ที่ 0/200/400/600/800/1000 ms ได้ on/off/on/off/on/off ด้วยสี {0,255,0} แล้วตั้งแต่ 1200 ms ขึ้นไปค้างที่ {0,40,0}
  - `test_led_off`: ได้ {0,0,0} ทุกช่วงเวลา
- [ ] **Step 2:** รัน `pio test -e native` ต้อง FAIL
- [ ] **Step 3:** Implement `computeLayout` และ `ledColorAt` ให้ตรงกับค่าใน test
- [ ] **Step 4:** รัน `pio test -e native` ต้อง PASS
- [ ] **Step 5: Commit:** `feat(core): grid layout and led patterns`

### Task 4: hook — logic สถานะ (pure)

**Files:**
- Create: `hook/lib/state.js`, `hook/package.json` (`{"type":"commonjs","private":true}`)
- Test: `hook/test/state.test.js`

**Interfaces:**
- Produces (CommonJS):
```js
eventToState(evt) -> 'idle'|'working'|'waiting'|'done'|'end'|null   // null = ไม่เปลี่ยน
sanitizeName(str) -> string            // ASCII พิมพ์ได้เท่านั้น ยาวไม่เกิน 20
assignName(base, liveSessions) -> string  // 'api', 'api#2', ...
prune(sessions, nowMs) -> {keep: Session[], drop: string[]}  // drop = ids
buildSnapshot(sessions, nowMs) -> {now, sessions:[{id,name,state,updated_at}]}
resolveHostConfig({env, configJson, cacheJson}) -> {host, cachedIp|null}
// Session = {session_id, name, base, state, created_at, updated_at}
```

- [ ] **Step 1: เขียน failing tests (`node --test hook/test`)**
  - `eventToState`:
    - `SessionStart`→idle, `UserPromptSubmit`→working, `PreToolUse`→working, `PostToolUse`→working, `PostToolUseFailure`→working
    - `PermissionRequest`→waiting
    - `Notification` ที่ `notification_type:'permission_prompt'`→waiting, `Notification` ที่ `notification_type:'idle_prompt'`→null, `Notification` ที่ไม่มี type แต่ `message` มีคำว่า "permission"→waiting
    - `Stop`→done, `SessionEnd`→'end', `SubagentStop`→null, ชื่อ event อื่น→null
  - `sanitizeName`: `'claude-monit'`→`'claude-monit'`; `'โปรเจกต์ใหม่'`→`'?'` (ยุบอักขระที่ไม่ใช่ ASCII ที่ติดกันให้เหลือ `?` ตัวเดียว); ชื่อ 30 ตัวอักษร→ตัดเหลือ 20; `''`→`'session'`
  - `assignName`: ไม่มี session base นี้อยู่→`'api'`; มี `api` อยู่→`'api#2'`; มี `api` กับ `api#3`→`'api#2'` (ใช้เลขว่างที่น้อยที่สุด)
  - `prune`: working ที่ updated_at เก่ากว่า 31 นาที→drop; waiting ที่เก่า 7 ชั่วโมง→keep; done ที่เก่า 8 ชั่วโมง 1 นาที→drop
  - `buildSnapshot`: เรียงตาม created_at, ไม่ส่ง `base`/`created_at` ออกไป, `now` = nowMs
  - `resolveHostConfig`: ลำดับคือ env `CLAUDE_MONIT_HOST` > `configJson.host` > `'claude-monit.local'`; ส่ง `cachedIp` กลับเฉพาะเมื่อ host ลงท้ายด้วย `.local` และ `cacheJson.host === host`
- [ ] **Step 2:** รัน `node --test hook/test` ต้อง FAIL
- [ ] **Step 3:** Implement ใน `hook/lib/state.js` ใช้ TTL เดียวกับ Global Constraints
- [ ] **Step 4:** รัน `node --test hook/test` ต้อง PASS
- [ ] **Step 5: Commit:** `feat(hook): pure state mapping, naming and snapshot`

### Task 5: hook — entry, sender และ installer

**Files:**
- Create: `hook/lib/store.js`, `hook/claude-monit-hook.js`, `hook/send.js`, `hook/install.js`
- Test: `hook/test/hook.test.js`, `hook/test/install.test.js`

**Interfaces:**
- Consumes: ทุกฟังก์ชันจาก Task 4
- Produces:
  - `store.js`: `readAll(dir) -> Session[]`, `write(dir, session)` (เขียนไฟล์ tmp แล้ว rename), `remove(dir, id)` โดย `dir` default เป็น `~/.claude-monit/sessions` และ override ได้ด้วย env `CLAUDE_MONIT_HOME` (base dir) เพื่อให้ test ใช้ได้
  - การใช้ entry: `echo '<hook json>' | node hook/claude-monit-hook.js`
  - การใช้ sender: `node hook/send.js '<snapshot json>'` ซึ่งอ่าน `~/.claude-monit/config.json`, เขียน `~/.claude-monit/host-cache.json` แล้ว POST ไปที่ `http://<host>/snapshot`
  - การใช้ installer: `node hook/install.js [--uninstall] [--settings <path>]` โดย default path คือ `~/.claude/settings.json`

- [ ] **Step 1: เขียน failing tests**
  - `hook.test.js` (ใช้ `CLAUDE_MONIT_HOME` เป็น tmpdir และ `CLAUDE_MONIT_HOST=10.255.255.1` ซึ่งเป็น IP ที่ route ไม่ได้):
    - `writes session file on UserPromptSubmit`: ไฟล์ `<id>.json` ต้องมี `state:'working'` และ `name` เท่ากับ basename ของ cwd
    - `SessionEnd removes file`
    - `second session same cwd gets #2`
    - `hook exits fast when board unreachable`: วัดเวลาด้วย `spawnSync` ต้อง < 300 ms, exit code 0, stdout และ stderr ว่าง
    - `malformed stdin exits 0 silently`
  - `install.test.js` (ใช้ settings ใน tmpdir):
    - `adds entries for all events, keeps existing hooks`: เตรียม settings ที่มี hook ของ pixel-agents ใน `Notification` แล้วรัน install ต้องยังมี hook เดิมครบ และมี entry ของเราใน `SessionStart, UserPromptSubmit, PreToolUse, PermissionRequest, Notification, PostToolUse, PostToolUseFailure, Stop, SessionEnd`
    - `idempotent`: install สองครั้งต้องมี entry ของเราแค่ event ละ 1 อัน
    - `uninstall removes only ours`
    - `aborts on invalid JSON`: exit code ≠ 0 และไฟล์เดิมต้องไม่ถูกแก้ (เทียบ bytes)
    - `creates backup`: ต้องมีไฟล์ `settings.json.claude-monit.bak`
- [ ] **Step 2:** รัน `node --test hook/test` ต้อง FAIL
- [ ] **Step 3: Implement `claude-monit-hook.js`**
  - ครอบทั้งไฟล์ด้วย try/catch แล้ว `process.exit(0)` ทุกกรณี
  - ขั้นตอน: อ่าน stdin → `eventToState` → ถ้าได้ null ให้จบโดยไม่ทำอะไร → เขียนหรือลบไฟล์ → `prune` แล้วลบไฟล์ที่ drop → `buildSnapshot` → spawn sender แบบไม่รอ:
```js
spawn(process.execPath, [path.join(__dirname, 'send.js'), JSON.stringify(snap)],
      { detached: true, stdio: 'ignore', env: process.env }).unref();
```
  - entry ใหม่: `created_at = updated_at = Date.now()`, `base = sanitizeName(path.basename(cwd))`, `name = assignName(base, live)`
  - entry เดิม: แก้แค่ `state` กับ `updated_at`
- [ ] **Step 4: Implement `send.js`**
  - `resolveHostConfig` → ถ้ามี cachedIp ให้ลอง POST ไปที่ IP นั้นก่อน ด้วย `fetch` + `AbortSignal.timeout(1500)`
  - ถ้าลองแล้วล้มเหลว หรือไม่มี cache: ถ้า host ลงท้าย `.local` ให้ `dns.promises.lookup(host,{family:4})` แล้วเขียน cache จากนั้น POST อีกครั้ง
  - ทุกความผิดพลาดให้เงียบ ห้าม throw ออกไป
- [ ] **Step 5: Implement `install.js`**
  - parse settings ถ้าไม่ผ่านให้ abort ด้วย exit 1
  - เขียน backup ก่อนแก้ไฟล์
  - แต่ละ event ให้ลบ group ที่มี hooks command เป็นของเราออกก่อน แล้ว push group ใหม่ `{matcher:'', hooks:[{type:'command', command:'node "<abs path>/hook/claude-monit-hook.js"', timeout:5}]}` (`--uninstall` จะลบอย่างเดียว)
  - เขียนไฟล์ด้วย `JSON.stringify(obj, null, 2)` แบบ atomic
- [ ] **Step 6:** รัน `node --test hook/test` ต้อง PASS ครบ
- [ ] **Step 7: Commit:** `feat(hook): hook entry, detached sender and settings installer`

### Task 6: เฟิร์มแวร์ — Clawd และการ render

**Files:**
- Create: `src/clawd.h/.cpp`, `src/display.h/.cpp`
- Modify: `src/main.cpp` (เปลี่ยนจาก smoke test เป็นโหมด demo)

**Interfaces:**
- Consumes: `Session`, `State`, `computeLayout`, `Layout`
- Produces:
```cpp
void drawClawd(TFT_eSprite& spr, int x, int y, int scale, State st, uint8_t frame, bool dim);
// x,y = มุมซ้ายบนของกรอบ 16x14*scale
class Display {
 public:
  void begin();                                   // tft init, rotation 1, sprite 8-bit 320x240
  void renderSessions(const Session* const* v, int n, uint32_t nowMs);
  void renderEmpty(const char* host, const char* ip, uint32_t nowMs);
  void renderSetup(const char* apName);           // วิธีต่อ WiFi
  void renderMessage(const char* line1, const char* line2);
  int  hitTest(int x, int y) const;               // index ช่องจาก layout ล่าสุด หรือ -1
};
```

- [ ] **Step 1: เฟรม Clawd ใน `clawd.cpp`**
  - กริด 16×14 (x,y นับจากมุมซ้ายบน) ขาเหมือนกันทุกท่า: 4 ขา x=3,6,9,12 y=10–12 กว้าง 1
  - **Waiting**
    - ลำตัว (2,2)-(13,9); แขนซ้าย (0,5)-(1,6); ตาสูง 3 px ที่ (4,4)-(5,6) และ (10,4)-(11,6)
    - เฟรม 0: แขนขวาชูขึ้น (14,0)-(15,5)
    - เฟรม 1: แขนขวาเฉียงลง (14,3)-(15,6) และทั้งตัวเลื่อนขึ้น 1 px
  - **Done**
    - ลำตัว (2,2)-(13,9); แขนชูทั้งสองข้าง (0,1)-(1,5) และ (14,1)-(15,5); ตายิ้มเป็นเส้น 2×1 ที่ (4,5) และ (10,5)
    - เฟรม 1 ทั้งตัวเลื่อนขึ้น 3 px
  - **Working**
    - ลำตัว (2,2)-(13,9); แขน (0,6)-(1,7) และ (14,6)-(15,7); ตา 2×2 ที่ (5,4) และ (11,4) (มองขวา)
    - เฟรม 1: เลื่อนขึ้น 1 px และตาเลื่อนไปที่ (4,4),(10,4)
  - **Idle**
    - ลำตัวยุบ (2,4)-(13,9); แขน (0,7)-(1,8) และ (14,7)-(15,8); ตาปิดเป็นเส้น 2×1 ที่ (4,6) และ (10,6)
    - ทั้งสองเฟรมเหมือนกัน แต่วาด "z" ลอยอยู่มุมขวาบนของช่อง โดยเลื่อน y ตามเฟรม
  - วาดด้วย `fillRect` ทีละบล็อกขนาด scale×scale สีตัว `#d97757` ตา `#141413` ถ้า `dim` ให้ใช้สีตัว `#6b3b2b`
  - ความเร็วเฟรม: waiting 2.5 fps, done 4 fps, working 5 fps, idle 1 fps
- [ ] **Step 2: Implement `Display`**
  - render ทั้งจอลง sprite 8-bit แล้ว `pushSprite(0,0)` (sprite 16-bit ขนาด 150KB อาจ allocate ไม่ได้ใน RAM)
  - แต่ละช่อง:
    - waiting ใช้พื้นช่อง `#3a2a12` ส่วนช่องอื่นใช้พื้นหลัง
    - วาดเส้นกรอบ 1px สี `#2a2a28`
    - วาด Clawd กึ่งกลางแนวนอน ค่อนไปด้านบน
    - บรรทัดล่างเป็น `name` ตามด้วยสถานะ `WAITING!`/`done`/`working...`/`zzz` ใช้สีตาม Global Constraints ถ้าเป็น waiting ข้อความกะพริบ 1 Hz
    - ใช้ font 2 เมื่อ layout มี ≤2 ช่อง และ font 1 เมื่อมีมากกว่า
    - ถ้าชื่อกว้างเกินช่อง ให้ตัดแล้วต่อท้ายด้วย `..`
  - `renderEmpty`: Clawd idle ×8 อยู่กลางจอ ใต้ภาพเป็น host กับ IP ใช้ font 2 สี `#666666`
- [ ] **Step 3: โหมด demo ใน `main.cpp`**
  - เมื่อ build ด้วย `-DDEMO_MODE` ให้สร้าง Snapshot ปลอมตามจำนวนที่วนเปลี่ยนทุก 5 วินาที (0,1,2,4,6,8) โดยครบทุก state แล้วเรียก `renderSessions` ที่ประมาณ 15 fps
  - เพิ่ม `[env:cyd_demo]` ที่ `extends = env:cyd` และมี `build_flags = ${env:cyd.build_flags} -DDEMO_MODE`
- [ ] **Step 4:** รัน `pio run -e cyd_demo -t upload` ต้องสำเร็จ
- [ ] **Step 5: ตรวจด้วยตา**
  - ทุก layout ไม่มีส่วนไหนล้นช่อง
  - ท่าทางตรงกับ mockup `character.html` แบบ A
  - จอไม่กะพริบ
  - ช่อง waiting เด่นที่สุด
  - ถ้าต้องแสดงผลให้ผู้ใช้ดู ให้ถ่ายรูปหรือให้ผู้ใช้ยืนยัน
- [ ] **Step 6: Commit:** `feat(fw): clawd pixel sprites and grid renderer with demo mode`

### Task 6b: วันที่และเวลา (เพิ่มตามคำขอผู้ใช้)

**Files:** Create `lib/monit_core/clock_fmt.h/.cpp`; Modify `lib/monit_core/layout.h/.cpp`, `src/display.h/.cpp`, `src/main.cpp`, `platformio.ini` (`-DLOAD_FONT7`); Test `test/test_core/test_main.cpp`

**Interfaces:**
- `constexpr int STATUS_BAR_H = 18;` ใน `layout.h` — `computeLayout` วางช่องใน y ∈ [18, 240)
- `struct ClockText { char date[16]; char time[6]; };` และ `ClockText formatClock(const struct tm* t, bool colonOn);` — `t == nullptr` (ยังไม่ซิงก์) → date `""`, time `"--:--"`; colonOn=false → `"14 32"`
- `Display::renderSessions(..., const ClockText& clk)` / `renderEmpty(host, ip, clk, nowMs)` รับเวลาที่ฟอร์แมตแล้ว
- Tests: `test_layout_below_status_bar` (cells[0].y == 18, ช่องล่างสุด y+h == 240, count=6 แถวสอง y == 18+111), `test_clock_format` (2026-10-25 14:05 Sunday → "Sun 25 Oct" / "14:05"; colonOn=false → "14 05"), `test_clock_unsynced`

### Task 6c: สภาพอากาศและสัญญาณ WiFi (เพิ่มตามคำขอผู้ใช้)

**Files:** Create `lib/monit_core/weather.h/.cpp`, `src/weather_task.h/.cpp`, `src/icons.h/.cpp`; Modify `src/display.*`, `src/main.cpp`
**Interfaces:**
- `int wifiBars(int rssi)`; `enum class WeatherIcon {Sun, PartlyCloudy, Cloud, Rain, Storm}`; `WeatherIcon weatherIconFor(int wmoCode)`; `struct Weather { bool valid; int tempC; int code; }`; `bool parseWeather(const char* json, size_t len, Weather& out)` (อ่าน `current.temperature_2m` ปัดเป็นจำนวนเต็ม และ `current.weather_code`)
- `struct StatusInfo { ClockText clk; Weather weather; bool wifiUp; int wifiBars; }` ส่งให้ `renderSessions`/`renderEmpty` แทน `ClockText`
- Tests: `test_wifi_bars`, `test_weather_icon_mapping`, `test_parse_weather` (31.6 → 32, -0.4 → 0; JSON ไม่มี current → false)

### Task 7: เฟิร์มแวร์ — WiFi, HTTP, ทัช และ LED

**Files:**
- Create: `src/net.h/.cpp`, `src/touch.h/.cpp`, `src/led.h/.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `SessionStore`, `parseSnapshot`, `ledColorAt`, `Display`
- Produces:
```cpp
// net
void netBegin(Display& d);              // WiFiManager autoConnect("claude-monit-setup"), MDNS "claude-monit"
void netLoop();
void netOnSnapshot(bool (*cb)(const char* body, size_t len)); // cb false -> HTTP 400
String netStatusJson();                 // ใช้ตอบ GET /status
// touch
bool touchRead(int& x, int& y);         // พิกัดจอ (ใช้ min/max จาก hw_pins.h), debounce 300 ms
// led
void ledBegin(); void ledSet(Rgb c);    // ใช้ analogWrite แบบ active-low (เขียนค่า 255-v)
```

- [ ] **Step 0:** หลังต่อ WiFi ได้ เรียก `configTzTime("ICT-7", "pool.ntp.org", "time.google.com")`; main ใช้ `getLocalTime(&tm, 0)` ถ้าได้ false ส่ง nullptr ให้ `formatClock`
- [ ] **Step 1: `net.cpp`**
  - ก่อนเรียก `autoConnect` ให้ตั้ง callback `setAPCallback` ที่เรียก `d.renderSetup("claude-monit-setup")`
  - ตั้ง `setConfigPortalTimeout(180)` ถ้าหมดเวลาให้แสดง `renderMessage("WiFi failed","restarting...")` แล้ว `ESP.restart()`
  - endpoint:
    - `POST /snapshot`: body สูงสุด 8 KB ถ้าเกินให้ตอบ 413
    - `GET /status`: ตอบ JSON ของ store
    - `GET /`: ตอบข้อความ "claude-monit ok"
  - ถ้า WiFi หลุดใน loop ให้ `WiFi.reconnect()` ทุก 10 วินาที โดยจอยังวาดต่อไปตามปกติ
- [ ] **Step 2: `main.cpp` (โหมดจริง)**
  - ลำดับใน `setup`: `display.begin()` → `ledBegin()` → `netBegin()`
  - ใน loop:
    - `netLoop()`
    - `store.expire(millis())` ทุก 1 วินาที
    - ถ้าทัช ให้ `hitTest` → acknowledge `id` ของช่องนั้น
    - ประมวลผล LED:
      - mode = `store.ledMode()`
      - จับเวลา `msInMode` ใหม่เมื่อ mode เปลี่ยน หรือเมื่อ `doneGeneration()` เพิ่มขึ้น
      - เรียก `ledSet(ledColorAt(...))`
    - render ที่ประมาณ 15 fps: ถ้า `store.count()==0` ให้ใช้ `renderEmpty("claude-monit.local", WiFi.localIP().toString().c_str(), now)` ถ้าไม่ใช่ให้ใช้ `renderSessions`
- [ ] **Step 3:** รัน `pio run -e cyd -t upload` ต้องสำเร็จ แล้วเอามือถือต่อ AP `claude-monit-setup` เพื่อใส่ WiFi (ขั้นนี้ให้ผู้ใช้ทำเอง)
- [ ] **Step 4: ทดสอบด้วย curl**
  - `curl -s http://claude-monit.local/` ต้องได้ `claude-monit ok`
  - `curl -s -X POST http://claude-monit.local/snapshot -d '{"now":1000,"sessions":[{"id":"a","name":"api","state":"waiting","updated_at":900}]}'` ต้องได้ HTTP 200 จอมีช่อง api สีส้ม และ LED ส้มกะพริบ
  - `curl -s -o /dev/null -w '%{http_code}' -X POST http://claude-monit.local/snapshot -d 'garbage'` ต้องได้ `400`
  - แตะช่อง api ช่องต้องเป็น idle และ LED ดับ
- [ ] **Step 5: Commit:** `feat(fw): wifi setup portal, http snapshot api, touch ack and led`

### Task 7b: กดค้างดูรายละเอียด (เพิ่มตามคำขอผู้ใช้)

**Interfaces:**
- hook `state.js`: `toolFromEvent(evt) -> string|undefined`, `displayCwd(cwd, home) -> string`; snapshot entry เพิ่ม `cwd`, `tool` (สตริงว่างถ้าไม่มี)
- core: `SnapshotEntry`/`Session` เพิ่ม `char cwd[44]; char tool[24];` (optional ใน JSON); `void formatAge(uint32_t ms, char* out, size_t n)` → `"just now"` (<60 s), `"5 min"`, `"2 h 10 min"`; `class TouchGesture { TouchEvent update(bool touched, int x, int y, uint32_t nowMs); }` กับ `enum class TouchEvent {None, Tap, LongPress}` และ `LONG_PRESS_MS = 600`
- fw: `Display::renderDetails(const Session&, const StatusInfo&, uint32_t nowMs)`
- Tests: `toolFromEvent`, `displayCwd`, `buildSnapshot` มี cwd/tool, hook เก็บ tool/cwd; core `test_parse_optional_details`, `test_format_age`, `test_gesture_tap`, `test_gesture_long_press_once`

### Task 8: ติดตั้ง hook และทดสอบจริงแบบ end-to-end

**Files:**
- Create: `README.md` (วิธีแฟลช, ตั้งค่า WiFi, install/uninstall hook, ตั้ง `CLAUDE_MONIT_HOST` และวิธีดู `/status`)

- [ ] **Step 1:** แสดง diff ของ `~/.claude/settings.json` ให้ผู้ใช้ดูก่อน แล้วขออนุญาต จากนั้นรัน `node hook/install.js` แล้วตรวจด้วย `node -e` ว่ามี entry ใหม่ครบ 9 event และ hook เดิม (pixel-agents, cc-status, superset) ยังอยู่ครบ
- [ ] **Step 2:** ถ้า `claude-monit.local` resolve ไม่ได้ ให้เขียน `~/.claude-monit/config.json` เป็น `{"host":"<IP บนจอ>"}`
- [ ] **Step 3: ให้ผู้ใช้เปิด Claude Code 2 session ในโฟลเดอร์ต่างกัน และอีก 1 session ในโฟลเดอร์ซ้ำ แล้วไล่ทดสอบ**
  - ต้องมี 3 ช่อง และหนึ่งในนั้นชื่อ `#2`
  - พิมพ์ prompt แล้วช่องต้องเป็น working
  - สั่งคำสั่งที่ต้องขออนุญาต ช่องต้องเป็น waiting และ LED ส้ม
  - กดอนุญาต ช่องต้องกลับเป็น working
  - เมื่อทำงานจบ ช่องต้องเป็น done และ LED เขียว
  - แตะช่อง done ช่องต้องเป็น idle
  - ปิด session ช่องต้องหายไป
- [ ] **Step 4:** ถอดปลั๊กบอร์ดแล้วเสียบใหม่ เมื่อ session ใดก็ได้มี event ถัดไป จอต้องกลับมาแสดงครบทุก session
- [ ] **Step 5: Commit:** `docs: readme and end-to-end setup`
