# Shared kit 2.1.0

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
