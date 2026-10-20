# Watchface kit 1.3

Established by `pebble-mondrian` on 18 October 2026. No earlier sibling kit
existed. This is a copied-source kit, with no runtime dependency on the lab,
Signal Deck, another watchface, or a server of our own.

## Integration

Copy `src/c/kit/` and `src/pkjs/kit/` into the next face. Keep the lab contract
(`lab.h`, `lab_common.c`, scenario definitions in `lab_data.c`, `lab_designs.h`).
Kit additions to `LabData` must be copied too: temperature unit, optional label
size, tomorrow/next sun fields, maximum UV, rolling temperature/rain/code,
rolling local hour/day labels, `rolling_count` and `hr_range_partial`.

`lab_main.c` calls `kit_shell_run(&your_design)`. The live branch of
`lab_data_refresh` sets local time/device state and calls `kit_fill`; fixed lab
scenarios bypass the kit. Use `src/pkjs/index.js` as the small app bootstrap.
It wires Clay and the kit modules; change the app heading and storage namespace.
Add `@rebble/clay`, health/location/configurable capabilities and the message keys
below to package.json. Build live by default (`LAB_SCENARIO=3`).

The host renderer defines `LAB_HOST_RENDER`, compiles C recursively and excludes
`lab_main.c`. All watch-only kit modules are guarded accordingly. `kit_core.c`
is pure C and also runs in native tests. Keep `LabDesign.draw`/`draw_detail`
unchanged. No button handlers are needed for a watchface.

## Files

| File | Responsibility |
|---|---|
| `kit.h` | watch-side integration API and persisted settings |
| `kit.c` | single AppMessage dispatcher; registers before opening buffers |
| `kit_core.c/.h` | versioned weather decoder, validation, calendar, DST and retry state |
| `kit_health.c` | permission-aware health, event coalescing, daily/hourly history caches |
| `kit_hr_history.c` | bounded asynchronous recorded-minute HR reconstruction |
| `kit_settings.c` | staged settings validation, persistence and world-clock projection |
| `kit_weather.c` | request correlation, watchdog/retries, cache and LabData projection |
| `kit_shell.c` | window, minute tick, touch/flick detail view and frame timing |
| `pkjs/kit/config.js` | Clay schema, including optional larger captions |
| `pkjs/kit/settings.js` | settings normalization and phone storage |
| `pkjs/kit/queue.js` | one native acknowledged send at a time, FIFO, three attempts |
| `pkjs/kit/weather.js` | serialized geolocation/HTTP, Open-Meteo validation and wire encoding |

## Defaults and keys

Watch persistent key **1000**: 12-byte versioned settings. Key **1001**: 204-byte
weather wire record. Both are private to each app UUID. Preserve these within an
app; bump the record version before changing layouts.

Settings are one complete dictionary: `TIME_MODE` (0 watch, 1 twelve-hour,
2 twenty-four-hour), `TEMP_UNIT` (0 C, 1 F), `STEP_GOAL` (1000–50000, default
10000), `WORLD` (0 NZ, 1 UTC, 2 LON, 3 NYC, 4 LA, 5 TKY, 6 SYD),
`WEATHER_ENABLED` (default 1), `DEMO_FALLBACK` (default 0), `LARGE_LABELS`
(default 0 for Mondrian, preserving Liam's preferred original artwork).
Partial/invalid dictionaries are ignored. Settings apply immediately and persist.
The phone sends this snapshot with `READY=1` before weather work.

Watch → phone: `REQUEST_WEATHER` is a positive request ID. Phone → watch:
`WEATHER_ID` plus either `WEATHER` (byte array) or `WEATHER_ERROR=1`.
Only replies for the active request can affect the watch. AppMessage buffers are
512 bytes inbound and 64 outbound; weather and settings are separate messages.
SDK message keys are extern values, so arrays of them cannot be static C
initializers. Clean/rebuild when the SDK caches an old message-key schema.

## Weather format and reliability

All multibyte values are little endian. The 204-byte record fits Pebble's
256-byte limit per persisted value. It is decoded into a temporary snapshot
and committed only after every field and the FNV-1a checksum pass.

| Bytes | Contents |
|---|---|
| 0–3 | `MK`, version 1, hourly count (24–48) |
| 4–11 | observation epoch, hourly start epoch |
| 12–19 | two local forecast dates, YYYYMMDD |
| 20–35 | day 0 rise/set, day 1 rise/set, UTC epochs |
| 36–44 | signed Celsius temperature, WMO code, humidity, UV×10, wind km/h, direction |
| 45–52 | two signed daily highs/lows, two maximum UV×10 values |
| 53–100 | 48 signed Celsius hourly temperatures |
| 101–148 | 48 hourly rain percentages |
| 149–196 | 48 hourly WMO codes |
| 197–199 | reserved, zero |
| 200–203 | FNV-1a of bytes 0–199 |

Open-Meteo requests `forecast_days=2`, `timezone=auto`, `timeformat=unixtime`.
Current UV comes from the corresponding hourly UV sample; maximum UV comes from
the daily response. Nulls are rejected rather than converted into zero. Polar
sun events may be an explicit pair of zero values. Dates apply the API's UTC
offset as documented; sun/hourly timestamps remain UTC.

The watch alone schedules: startup after settings (five-second fallback), then
30 minutes after success. One 45-second watchdog covers location/HTTP/delivery.
Failures retry after 1, 5 and 15 minutes; a fourth failure returns to 30 minutes.
Each timer/request is cancelled or made irrelevant when weather is disabled.
The phone uses 15-second location and HTTP limits. Native AppMessage ACK/NACK,
not a JS timeout, ends a send; otherwise retries can overlap a still-active send.

The last valid observation persists through failure/restart for at most three
hours. Expiry clears every projected weather field together. At midnight, daily
high/low/maximum UV switch to day 1; future sun epochs select tomorrow's sunrise.
Rolling arrays use actual chronological hours. `rolling_count` can fall below 24
near the end of an older cache, so never wrap or repeat missing forecast data.
Use `rolling_hour[i]`/`rolling_day[i]` for labels across DST transitions.

## Health, timing and touch

Health subscriptions never request a faster heart-rate sampling period. Movement,
sleep and historical queries coalesce to one refresh per minute. Every heart-rate
event captures the current filtered value, including later events in the same
minute; no health event marks the canvas dirty. A significant update invalidates cached historical days.
Completed hourly bins refresh once an hour; current-hour values refresh once a
minute. Permission loss clears data, and demo fallback is explicitly opt-in.

Pebble `mktime` does not resolve `tm_isdst=-1`: it uses the supplied UTC offset.
Historical local boundaries therefore resolve the target offset with `localtime`
instead of reusing today's `tm_gmtoff`. Repeated hours share their wall-hour bin;
skipped boundaries normalize forward. The kit's world clock uses UTC transition
instants for current NZ/LON/US/SYD rules, not date-only DST flags.

Minute ticks only; no animation. Touch or wrist flick toggles detail, with an
eight-second timeout. Default touch subscription is continuous to preserve direct
tap behavior. `KIT_TOUCH_WINDOW_MS=10000 pebble build` provides an A/B hardware
build subscribing for ten seconds after startup/flick. **Touch power cost has not
been measured on a physical watch.** Decide between these policies using real
hardware battery measurements, not QEMU CPU usage.

The shell logs `Frame face/detail Nms`. Emulator results and remaining hardware
checks are recorded in the app README. No static bitmap cache is needed for this
rectangle-only design unless physical-watch timing demonstrates otherwise.

Battery/connection callbacks must apply their supplied state after refreshing
other data. A `*_peek()` made inside the callback can still report the previous
state, making exception indicators lag one event. The final QEMU checks exposed
this; `kit_shell.c` uses callback values directly.
ConnectionService also deliberately hides disconnects for 25 seconds. Wait at
least 27 seconds in QEMU before checking the phone-lost marker; reconnecting
earlier cancels the notification. This is firmware debounce, not a rendering lag.

## Verification and changelog

1.3 (20 October 2026): combine Mondrian 1.0.6's history reader with
Underground 1.2's pace formatter. Reconstruct today's HR extrema and hourly
averages from
`health_service_get_minute_history`, not midnight-to-now aggregate queries.
The two-hour aggregate API limit does not bound this separate minute-file path.
A 60-record stack buffer is read in 25ms timer batches; no day-sized allocation,
extra redraw, sensor-period change or own persisted health database is needed.
Completed minutes are scanned once, then only new minutes are appended. A
significant health sync, local-day/time-zone change or clock rollback rebuilds
the cache. Repeated DST hours combine and a missing hour stays empty.
Ignore invalid records and BPM 0/255. Continue short reads/gaps using returned
start/count, validate SDK bounds, and never trust a mutable end time as progress.
Permission loss cancels reads and clears the range. No current sample is required
if earlier recorded values exist. A missing window/read failure is retried during
an hourly rescan; a timer allocation failure is retried at the next refresh.
`hr_range_partial` covers loading, invalid/missing records and read failures.
Mondrian uses `BPM*` while partial; Underground appends `*` to the range
on its Heart status row. Missing extrema show `range --` (or `range --*` while
loading/incomplete). Recorded history remains visible without a current sample. Values represent
recorded minute BPMs, so sub-minute peaks are not promised.
Validation: the real-module history fixture reconstructs noon extrema from
morning records older than two hours, resumes short reads, handles failures and
missing records, repairs coverage on hourly rescan, and handles 23/25-hour local
days. Health permission-loss tests also assert that pending reads are cancelled.
Underground 1.0.3 preserves its pace wording, transit geometry, phone namespace
and existing watchfaces-list icon. The inherited aggregate HR limit applies to
aggregate queries; it does not impose a two-hour limit on minute-file history.
Asterisk means incomplete/loading history, not an abnormal heart rate.

Always stop QEMU after a check, including on failure. The reusable
`tools/emulator_history_check.py --clean-profile` (run with pebble-tool's Python)
backs up/restores the existing emulator profile, verifies this app's active UUID,
checks minute-history completion and tap timeout, and kills QEMU in finally.
Recorded-minute persistence on physical Time 2 hardware remains a device check.


1.2 (19 October 2026), in `pebble-underground`: copied Mondrian's 1.1 kit,
including the filtered-current-HR fix. Added SDK-independent
`kit_format_pace(out,size,steps,usual)` in `kit_core`: explicitly says
`ahead of usual`, `behind usual`, `At usual pace`, or `No usual pace data`.
Do not treat an unavailable baseline as "ahead". Formatting is covered by the
native core tests. The weather/settings wire format and persist versions stay 1.
The startup log is now face-independent.

The copied Clay schema is this face's shared settings UI; the Mondrian-only
larger-caption toggle is removed. `LARGE_LABELS=0` remains an internal protocol
field so complete settings dictionaries and persisted records remain compatible.
Phone storage uses `underground-kit-settings-v1`. A later face should set its own
heading/namespace and expose only its requested settings.

Consumer gotchas caught while porting Underground:
- Use one rounding convention for the displayed current UV and its band. Tests
  include the fractional boundaries 2.5/5.5/7.5/10.5. Night clamps current UV to
  zero, while the independent daily-peak marker remains visible.
- A valid current BPM does not guarantee a min/max range. Show `range --` when
  history is unavailable.
- `weather_ok` does not guarantee sun events: polar observations can be valid
  with `next_sun_min=-1`. Never compute a negative sun countdown.
- Rolling rain labels use `rolling_hour` and `rolling_day`; `+1` marks tomorrow.
  After sunset the face also labels tomorrow's sunrise with `+1`, using the
  next day's actual time rather than repeating today's.
  Search only the available `rolling_count`, without wrapping a short cache.
- Measure the full board detail string before adding an ETA. Explicit pace
  wording takes priority over a due time when both cannot fit.


Underground final verification: 57 host-rendered scenes and the inherited kit
suite passed, including the new pace formatter and 143,192 independent DST
comparisons. The demo's unchanged upper bowl is pixel-identical to the lab.
Final Emery screenshots/logs verify the actual Underground UUID, filtered HR,
steps, board toggle/eight-second return, critical battery, phone disconnect and
reconnect, live 12-hour/Fahrenheit settings through Clay's response path,
correlated synthetic weather delivery, and cache persistence after restart.
`tools/verify_emulator.py` asserts timeout/toggle identity, rendered status-chip
pixels and cached map identity after restart. Frames took 3–17 ms in QEMU;
clean exit reported zero remaining allocations. Physical-watch timing and touch
power remain unmeasured. The final emulator was stopped after verification.

Emulator launch gotcha: an install-success message is insufficient. An existing
watchface can remain active. `tools/launch.py` explicitly starts the package UUID,
keeps its async transport alive, then queries and asserts the active UUID before
screenshots. Closing the transport immediately after the launch packet can leave
the old face on screen. Never accept another app's screenshots as verification.

1.1 (19 October 2026): align current HR availability with Signal Deck:
`health_service_metric_aggregate_averaged_accessible(HeartRateBPM,now,now,Avg,Once)`
then `health_service_peek_current_value`. Never gate a current value on today's
history. PebbleOS limits HR history queries to two hours, so midnight-to-now
can be unsupported after 02:00 even with a valid current reading. The explicit
aggregation query also avoids older firmware's generic HR accessibility issue.
Capture each HR event without repeating history queries or redrawing the face.
Regression tests compile the real `kit_health.c` against a controlled SDK stub:
after-02:00 availability, midnight rollover, same-minute zero-to-valid/multiple
samples, permission loss/restoration and OS-expired readings. The original code
fails this regression; the fix passes. No stale-value cache or raw BPM fallback
was added. The OS's filtered reading already has its own freshness policy.
Final 1.0.4 Emery smoke check showed a valid current filtered BPM at 11:45,
with the heart tile pixel-identical to the matching host render. Face/detail
frames took 6–10 ms, with no app faults. Captures and logs are in
`shots/health-fix-*`. Synthetic HR injection requires initializing emulator
health totals first (`pebble emu-steps 6842`); the OS's filtered BPM may differ
from the injected raw sample. The clock was restored after testing.
The old daily aggregate limitation is superseded by the minute-history reader
in kit 1.3; unavailable history must not hide a valid current BPM.


1.0: first kit, established in Mondrian. Tests are `python3 tests/run.py`:
cross-language weather encoding/decoding, atomic malformed-payload rejection,
expiry boundaries, retry/watchdog scheduling, phone serialization, persisted
settings normalization and exact DST transition tests. Independent Python
zoneinfo comparisons cover every six hours in 2024–2037 for all seven presets.
`tools/render_scenes.py` covers layout/units/status/weather variants without QEMU.

Final Emery check passed: live settings via the Clay response path, correlated
weather transport/cache, restart, injected steps/heart sample, detail toggle and
eight-second timeout, amber/charging states. Observed draw times were 2–11 ms;
the exit log reported zero remaining allocations. Test logs are under `shots/`.
The default demo is pixel-identical to the original proposal. Liam preferred
that composition to the readability redesign, so larger captions are optional.
The final battery/charging/phone-lost regions also match their host-rendered
counterparts pixel for pixel. `tools/verify_emulator.py` verifies those captures,
restored connectivity and the weather cache after restart without another reply.

Emulator gotcha: a running C face can have stale phonesim lifecycle state even
when installation succeeds. Sending `AppRunStateRequest` refreshes it and starts
its companion JS. A synthetic config response also requires `AppConfigSetup`
first, so phonesim has a pending configuration callback. The test harness uses
both handshakes. It restores the original settings and stops its log process.
Phonesim's native GeoIP lookup can block configuration callbacks in this SDK;
the injection test keeps phone weather disabled until restoring defaults, while
exercising the watch request/response path separately. It injects only the first
weather reply, so the restarted face must use its persisted cache.

Future faces: copy the highest-version sibling kit, update only your copy, bump
this version and record fixes here. Keep Signal Deck read-only.

Release baseline: Mondrian 1.0.3 is the initial public release with corrected
package metadata. Publishing metadata changes leave the copied-source kit at
version 1.0.

Release gotcha: after changing package.json version, use a clean build before
uploading and inspect appinfo.json inside the PBW. An incremental SDK build
can regenerate build/appinfo.json while leaving the previous PBW unchanged.

For later uploads, pass `--replace-screenshots` when sending the same approved
assets again; otherwise the CLI appends duplicate screenshots to the listing.

Emulator cleanup: once the final check is complete, run `pebble kill` and
verify QEMU has stopped. Do not leave the emulator running between tasks.

## Underground release audit — 19 October 2026

Before the initial public release 1.0.1, compared against Mondrian's clean local
checkout and verified that GitHub's latest commit was the same
`8b40d001a8169d8bdf2ea23766bb4fe558e9ef8c` (Mondrian 1.0.4 / kit 1.1).
`kit_health.c`, `kit_weather.c`, `kit_settings.c`, `kit.c`, phone weather/queue
and bootstrap, lab contract/common helpers, wscript and HR regression tests
match that baseline byte for byte. The remaining kit differences are the
intentional 1.2 pace formatter, generic startup log, this face's settings
heading/namespace and omission of Mondrian's optional caption toggle.
No upstream improvements were missing; no sibling files were edited. The full
kit suite, including filtered-HR regressions and 143,192 zoneinfo comparisons,
passed again before publication. The approved render pixels remain unchanged.

Underground 1.0.1 published on 19 October 2026: https://apps.repebble.com/c977d7b7e07b49298007a427.
The public listing shows both approved Emery screenshots. Downloaded the public
PBW and verified every ZIP entry against the local release; all contents match.
Publication receipt and artifact verification are recorded in `store/`.


1.0.3 validation (20 October 2026): all protocol, health and minute-history
tests passed, including older records, gaps, permission loss, incremental reads
and 143,192 timezone comparisons. All 62 render scenes passed; the original
upper bowl remains pixel-identical. Final clean-profile Emery testing verified
the active Underground UUID, current BPM 64 and the partial recorded-minute
range 64–64*, then returned after eight seconds to an identical main-face
capture. No app faults were logged. Most frame wall-clock measurements were
3–15ms, with one 1005ms measurement at the first minute boundary; these QEMU
wall-clock logs are not physical-watch CPU timings. The SDK profile was restored
and the emulator stopped. Evidence: `shots/history-*`.
