# Pebble watchface kit

Version 2.2.0 adds shared calculations, live-data assembly and emulator-test
utilities for Mondrian, Underground, Meridian, Word Clock and Quest. The kit
focuses on data, behaviour and test support; drawing and artwork stay in the
faces. Fresh goals default to 8,000 while valid saved goals are preserved. See
[src/c/kit/KIT.md](src/c/kit/KIT.md).

Shared C and PebbleKit JS for standalone Pebble watchfaces. Version 2.0.0 extracted
Underground kit 1.3 (including Mondrian's recorded-minute HR history fix) into
one repository. Each PBW includes the code it needs; there is no shared runtime.
Signal Deck has not been migrated.

Consumers keep a `shared-kit` Git submodule. The `src/c/kit`, `src/pkjs/kit`,
`src/c/lab.h` and `src/c/lab_common.c` paths are symlinks into it. They contain
no forked implementation. These links work with the Pebble SDK, native tests
and Watchface Lab renderer. Clone with Git symlink support enabled (the default
on macOS and Linux).

Each consumer owns `src/pkjs/app-config.js`: its heading, existing phone storage
key and optional larger-label UI. Settings and watch persistence retain their
original formats. Keep each consumer's messageKeys in their existing order.
`src/c/design.c`, scenarios, assets, UUID and app version remain consumer-owned.

## 2.1.0 extension

Version 2.1.0 adds optional face-specific Clay settings, weak C
init/receive hooks and optional drawing resources. Existing consumers retain
their common protocol, persistence and rendering. See
[src/c/kit/KIT.md](src/c/kit/KIT.md) for the interface, validation and release
requirements. Existing consumers do not need to opt into the extension.

## Building a watchface

```sh
git clone --recurse-submodules <watchface-url>
cd <watchface>
npm ci
pebble build                    # current kit checkout; no fetch
npm run release                 # fetch latest main, test, clean, build live
npm run release -- --render      # also check render coverage (Watchface Lab)
npm run release -- --pinned      # no fetch; rebuild the checked-out kit
```

For an existing clone, initialize once with `git submodule update --init`.
For historical builds use `git submodule update --init` after checking out the
watchface revision, then the pinned release command. The release command uses
the newest `main` on every run and fails if the fetch fails. It never silently
releases stale code, overwrites kit edits, or abandons unpublished kit commits.
Ordinary builds allow kit edits and label their build metadata as modified.

The release command runs shared and consumer tests, forces live data/default
touch, cleans the SDK build, validates the PBW's UUID/version and writes
`build/<watchface>.provenance.json` with the exact kit commit and PBW SHA-256.
It does not commit, push, install or publish a watchface. Commit the updated
consumer gitlink with the app release so its source records the dependency.
For a reproducible release also commit the app source; the receipt explicitly
marks an uncommitted app worktree rather than describing it as clean.

## Improving the kit while working on a watchface

Read the consumer's AGENTS.md and `shared-kit/AGENTS.md`. Edit shared behaviour
inside `shared-kit`, then test the candidate against both consumers. For example,
from this repository (pass explicit face paths if they are elsewhere):

```sh
python3 tests/run.py
python3 tools/check_consumers.py --render ../pebble-mondrian ../pebble-underground
```

Commit/share the tested kit change on this repository's `main` first. Each
consumer's next `npm run release` automatically fetches it. Use a kit branch
for work in progress; keep `main` suitable for releases. The deterministic kit suite and cross-consumer SDK/render checks run locally
with the existing SDK and Watchface Lab. They are required by AGENTS.md before
main. `ci/github-actions-tests.yml` is an optional Actions workflow template;
copy it to `.github/workflows/tests.yml` using a GitHub credential with workflow
permission to activate CI. The initial publishing token lacks that permission.

`docs/copied-kit-history.md` preserves historical protocol and validation notes.
Its old copy-to-next-face instructions are superseded by this repository.
MIT licensed. The kit does not bundle fonts or Clay; consumers own those assets
and install Clay through their existing package-lock.json.
