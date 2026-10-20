# Shared watchface kit

This repository is the single source of shared behaviour for Mondrian and
Underground. Signal Deck has not been migrated.

- Shared health/history, weather/protocol/cache/retries, settings normalization,
  phone messaging, world-clock calculations, lifecycle/touch/timing, common
  rendering helpers and the LabData/LabDesign contract belong here.
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
