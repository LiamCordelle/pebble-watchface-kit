# Shared watchface kit

This repository is the single source of shared behaviour for Mondrian,
Underground, Meridian, Word Clock and Quest. Consumers may have local candidate
extensions; do not overwrite or treat those as shared releases. Signal Deck has
not been migrated.

- Decide ownership by reusability, not by where code currently lives or how
  many faces use it. General calculations belong here even for their first
  consumer. Moon phase/illumination calculations belong in the kit; moon
  artwork, placement and styling belong to a face. When a face update touches
  reusable local logic, extract it here and replace the local implementation
  with the shared API. Avoid unrelated refactors.
- Shared health/history, weather/protocol/cache/retries, settings normalization,
  phone messaging, world-clock calculations, lifecycle/touch/timing, shared
  calculations and emulator-test utilities and the LabData/LabDesign contract belong here.
- Layout, artwork, face-specific wording, assets, UUID, app version and exposed
  settings belong to each consumer. Add configuration or a small interface when
  behaviour varies; do not put consumer names or storage keys into kit code.
- Add regression coverage here for shared behaviour. Preserve the weather wire
  format, AppMessage key numbering, watch persistence layouts, existing phone
  storage keys and settings during compatible changes. Explicitly plan migrations
  for incompatible changes.
- Run `python3 tests/run.py`. Before sharing a kit revision on `main`, also run
  `python3 tools/check_consumers.py --render` against both local migrated faces.
  This checks candidate worktree changes in isolated copies; it does not switch
  either consumer's kit checkout. Install each built candidate in QEMU when
  runtime behaviour changes. Stop QEMU afterwards.
- If working inside a consumer's `shared-kit`, that directory is a Git submodule:
  commit/share the kit change in this repository first, then update the consumer
  gitlink. Never copy implementations back into the consumer.
- Release builds refuse dirty or unpublished kit work. Do not bypass that check
  to release a fix. Ordinary `pebble build` supports local kit development.
- `main` is the tested integration branch. Bump VERSION for kit releases and
  document changes. Do not publish watchfaces to the Pebble store unless asked.
- When updating a consumer, check/fetch the kit's latest `origin/main` at the
  start and use it in the new watchface version. `npm run kit:update` is the
  guarded update-only path for a clean dependency. Preserve dirty/unpublished
  work and integrate it before release; report network/integration blockers
  rather than silently retaining a stale kit. Re-read upgraded instructions,
  validate against the final kit SHA, and record the dependency gitlink and
  version/SHA in the consumer release/provenance. A pinned historical build is
  an explicit exception, not the default update workflow.

## Calculation and test APIs

`src/c/kit/kit_calc.h/.c` is SDK-independent calculation/formatting code;
`kit_data.c` projects live watch data, moon illumination and world time.
Do not duplicate these in consumers. Preserve explicit unknown daylight,
chronological/bounded forecast searches and 64-bit activity arithmetic.
Default goals are 8,000; valid persisted settings always win.

Keep new kit work focused on data, behaviour and test support. Existing drawing
helpers are retained for compatibility; artwork, layout, fonts and new drawing
policy stay in consumers.

`tools/emulator.py` owns safe test sessions, settings/weather transport and
byte-cursor log parsing. `tools/emulator_smoke.py` runs the common runtime
checks with pebble-tool's Python. Consumers keep thin entrypoints and their own
visual assertions. Never discard a slow frame from logs or restore a profile
until the owned emulator is confirmed stopped; retain recovery backups on failure.
