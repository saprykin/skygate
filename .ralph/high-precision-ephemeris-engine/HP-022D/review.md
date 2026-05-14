## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-022D
- Title: Atomically activate verified ephemeris data
- Source: IMPLEMENTATION_PLAN.md
- Base ref: ef160d31c3469c42ed451224d7b87a44eb2884d1
- Head ref: dec72173f3ab97f9c4d8eedcde4e5d124ca5dbf5

## Summary

The implementation adds `SkyEphemerisDataManager::activateVerifiedStagedUpdateSet()`, verifies staged update sets before activation, activates profile assets into a revision-scoped cache path, persists a new `EphemerisDataCacheSnapshot`, and emits active-data/revision signals only after persistence succeeds. Focused tests pass, but the update-set activation is not atomic when the target revision path collides with the active revision, and activated leap-second/Delta T assets are not exposed through the active snapshot. The task needs fixes before final verification.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Activation can overwrite active files before the update set commits

Severity: BLOCKER
File: `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
Lines/functions: `activateVerifiedStagedUpdateSet()`, lines 438-484

Problem:
`activateVerifiedStagedUpdateSet()` derives the final cache root directly from the caller-provided revision token, then activates each asset into that root one at a time before settings persistence succeeds. If the requested revision token matches the currently active revision, a successful early asset activation can overwrite a file already referenced by the active/persisted snapshot. A later asset failure or interrupted process then returns without changing settings or emitting signals, but the old active dataset on disk has already been partially replaced.

Why it matters:
HP-022D explicitly requires interrupted activation and activation failure to preserve the active data set. Per-file `QSaveFile` commits are atomic for each asset, but the update set is not atomic as a group when final paths are reused. This can leave the application believing the old revision is active while one or more files under that revision now contain new staged data.

Recommended fix:
Activate the complete update set into a unique temporary revision root and promote that root only after every asset succeeds and metadata can be persisted, or reject/avoid revision-token collisions with the current active snapshot unless every target file is already byte-identical. Add a regression test where an existing installed revision is active, the new request reuses that revision token, one asset is activated, a later asset fails, and both the active files and persisted snapshot remain unchanged.

### Finding 2: Activated leap-second and Delta T assets are not exposed as active data

Severity: MAJOR
File: `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
Lines/functions: `cacheSnapshotForActivatedProfile()`, lines 235-239; `SkyActiveEphemerisDataSnapshot::leapSecondTableAsset()` and `deltaTDataAsset()`, lines 254-261

Problem:
The activation path processes leap-second and Delta T assets and records their versions in the cache snapshot, but no active paths are persisted for those assets. The active snapshot still returns `std::nullopt` from `leapSecondTableAsset()` and `deltaTDataAsset()`, so consumers such as `loadLeapSecondTableFromSnapshot()` and `loadDeltaTDataFromSnapshot()` will treat the newly activated time data as missing even though the UI/settings metadata says it is installed.

Why it matters:
HP-022D requires a complete verified staged update set to be promoted into the application data cache. After a successful activation, kernel and EOP data are usable through the active snapshot, but leap-second and Delta T assets are not. That leaves the high-precision engine with incomplete active data and can force missing-data or degraded behavior while reporting installed versions.

Recommended fix:
Persist active paths for leap-second and Delta T assets in `EphemerisDataCacheSnapshot`, populate them during activation, and have `SkyActiveEphemerisDataSnapshot` load and return those text assets when installed data is active. Extend the successful activation test to assert that `leapSecondTableAsset()` and `deltaTDataAsset()` are present with the activated payload, not only that their versions were saved.

## Test assessment

Focused tests were added for successful activation, revision/signal changes after successful activation, a pre-activation failure that preserves existing state, and a null-settings-store persistence failure. They cover the happy path and some failure behavior, but they miss the partial update-set failure case after one asset has already been promoted and they do not verify leap-second/Delta T active snapshot exposure.

Commands run:
- `git diff HEAD^..HEAD --check`
- `cmake --build build-ralph --target skygate-ui-sky-ephemeris-data-manager-tests && ctest --test-dir build-ralph -R '^skygate-ui-sky-ephemeris-data-manager-tests$' --output-on-failure`
- `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests && ctest --test-dir build-ralph -R '^skygate-ephemeris-data-activation-tests$' --output-on-failure`

Both targeted test commands passed. I did not rerun the full suite during review; the implementation handoff reports the only full-suite failure as the pre-existing QML main-window footer popup test tracked by HP-058.

## Regression risk

Medium

The new API is localized to `SkyEphemerisDataManager`, but it controls persisted active ephemeris data. A failed or interrupted activation can corrupt the currently active revision on disk, and successful activation currently leaves time-data consumers without the installed leap-second and Delta T assets.

## Out-of-scope observations

- The existing full-suite `skygate-ui-qml-main-window-tests` footer popup toolbar failure remains unrelated to HP-022D and is already tracked by HP-058.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
