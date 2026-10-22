# Shared kit 2.1.1

2.1.0 adds optional consumer settings and optional drawing resources to 2.0.0.
Released on 22 October 2026 after shared native/phone tests, both existing
consumer build/render checks and Word Clock runtime verification.

## Consumer settings interface

A consumer's app-config.js may include `faceSettings`, an array of Clay inputs.
Each entry must have a unique consumer messageKey. Use `toggle` with a boolean
defaultValue, or `select` with numeric-string defaultValue and explicit numeric
min/max bounds. Add keys after the existing shared keys in package.json.

The kit adds a Watchface section, normalizes the entries, persists them inside
the consumer's existing phone namespace, supplies boolean values to Clay and
sends integer values through the existing serialized queue. Shared wire keys and
watch settings/cache formats are unchanged. Consumers without faceSettings keep
identical payloads and UI.

Optional C functions override weak defaults:

- `kit_consumer_settings_init(void)`: restore the consumer's record at startup.
- `kit_consumer_settings_receive(DictionaryIterator *)`: validate/apply the entire
  consumer record atomically; return true only if it changed. The shell redraws
  through the same callback as common settings. It is called for every inbox
  dictionary, so return false if the consumer settings are missing.

The consumer owns its persistence layout and key (use >=2000; shared keys occupy
1000–1001), defaults, validation and rendering interpretation. Guard watch APIs
with `#ifndef LAB_HOST_RENDER`; supply deterministic host-render settings.
Do not put face names, storage keys or layouts in the kit.

## Optional resources

Common helpers no longer require unrelated fonts/PDCs in every app. Without
RESOURCE_ID_WX_SUN / RESOURCE_ID_SUN_RISE their drawing helper is a no-op.
Missing pixel fonts return system Gothic 14 bold / 18 bold respectively, and
system fonts are never unloaded as custom fonts. Consumers using existing
resources retain their original path and rendering.

## Validation and gotchas

- Shared deterministic suite and optional-schema/bootstrap tests pass.
- Mondrian and Underground pass isolated SDK builds and 36 / 62 render scenes.
- Keep all common message keys and persistence structures stable.
- Changes live in the dependency, not copied consumer code. Commit/share the kit
  before recording its new gitlink in consumer release commits.
- Release builds intentionally reject dirty or unpublished kit changes; ordinary
  builds retain truthful modified provenance for review.
- Touch defaults remain inherited: always available, eight-second detail timeout;
  KIT_TOUCH_WINDOW_MS=10000 permits a flick-activated touch window for measurement.
  QEMU cannot measure physical touch-controller battery consumption.

Word Clock runtime validation: explicit stop/start persistence, Clay settings,
forecast/cache, synthetic health and status checks pass on Emery. Three starts,
three exits with zero app allocations, 4–16ms frames and no app faults. The saved
SDK profile was restored and QEMU stopped. Consumer-specific evidence lives in
Word Clock's shots/emulator directory; no other kit checkout was changed.

## 2.1.1 — text consumer settings (22 October 2026)

`faceSettings` also accepts Clay `input` items with a string `defaultValue`.
Values remain strings through Clay, storage and the serialized AppMessage queue.
Optional `normalizeFaceSetting(key, text)` lets the consumer validate text while
keeping its meaning in the face. The normalizer must return a string; otherwise
its default is used. `attributes.maxlength` caps the result (1–128 characters;
64 when omitted). Common numeric keys stay reserved and unchanged. The existing
weak C settings hooks restore/apply the consumer's text record, so no parallel
receiver interface or copied implementation is required. Quest uses this for
its eight-character name with watch persistence key 2000.

Regression tests cover text defaults, invalid types, custom normalization,
maximum length, namespace isolation, phone storage and Clay ready/save transport.
The release helper `tools/update_release.py` is unchanged and remains Python;
build metadata/receipts belong under the consumer's ignored build directory.

2.1.1 release validation: shared deterministic tests and workflow safeguards
pass; Mondrian/Underground isolated builds and 36/62 render scenes pass. Quest
passes all 76 renders with approved pixels unchanged. Its real Clay text setting,
12-hour/Fahrenheit modes, correlated weather, cold name/settings/cache restart,
status chips and eight-second detail timeout pass in Emery. Both explicit exits
leave zero allocations. Typical QEMU frame wall times are 6–28ms with one
1010ms outlier; physical watch CPU timing and touch power remain unmeasured.
The prior SDK profile was restored and QEMU stopped. Evidence is in Quest's
shots/emulator directory. The weather wire format and shared settings records
remain unchanged.
