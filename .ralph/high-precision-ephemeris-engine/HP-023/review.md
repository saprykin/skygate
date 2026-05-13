## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-023
- Title: Add CALCEPH kernel loading and selection provider
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 5c6eee3
- Head ref: ab69b37

## Summary

The implementation adds a `CalcephKernelProvider`, active snapshot kernel path plumbing, CMake wiring, and focused tests for selection, checksum, open failure, and validity range behavior. The provider is mostly well-scoped and does not compute coordinates, but the active data snapshot integration does not preserve or validate kernel asset identity. That leaves the provider able to treat whatever installed kernel path exists as whichever manifest asset was selected.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Active kernel snapshot can misidentify the selected manifest asset

Severity: MAJOR
File: `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
Lines/functions: lines 146-158, `SkyActiveEphemerisDataSnapshot::solarSystemKernelAsset`; also `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp` lines 190-238

Problem:
`SkyActiveEphemerisDataSnapshot::solarSystemKernelAsset()` accepts any requested `assetId`, then fabricates an `EphemerisKernelDataAsset` with that ID and the single stored `installedKernelPath`. The persisted cache only records path/version metadata, not the installed kernel asset/profile ID. `CalcephKernelProvider` then asks for the selected manifest asset ID and reports the returned path as that selected manifest asset without checking `snapshotAsset->id` against `manifestAsset->id`.

Why it matters:
Modern and optional DE441 kernels are different selectable assets. With this snapshot contract, an installed DE441 path can be exposed as DE440s, or an installed DE440s path can be exposed as DE441. With checksum verification disabled, or if size/checksum metadata is incomplete in a future test or manifest path, the provider can open and report the wrong kernel under the wrong version/range metadata. With checksum verification enabled, valid installed data for a different profile fails as a checksum mismatch instead of being reported as the requested kernel not installed. This undercuts HP-023's requirement to load the selected modern kernel and select optional DE441 only when the installed active data actually contains it.

Recommended fix:
Carry the installed kernel asset/profile ID through the active data cache and snapshot, return `std::nullopt` when the requested asset ID does not match the active installed kernel, and have `CalcephKernelProvider` reject any snapshot asset whose `id` differs from the selected manifest asset before using `activePath`. Add tests for modern-vs-DE441 mismatches and for a snapshot returning a different asset ID.

## Test assessment

`skygate-ephemeris-calceph-kernel-provider-tests` covers open/close, default modern selection, optional long-range selection when requested, unavailable runtime, missing manifest kernel asset, missing file path, checksum mismatch, open failure, and out-of-range status. It does not cover active snapshot asset-ID mismatch, and `SkyEphemerisDataManagerTests::installedDataStatusAndSnapshot()` does not assert the new `solarSystemKernelAsset()` behavior.

Commands run:
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-calceph-kernel-provider-tests` passed.
- `ctest --test-dir build-ralph --output-on-failure` ran 115 tests with 114 passing. The only failure was the pre-existing `skygate-ui-qml-main-window-tests` failure at `QmlMainWindowTests.cpp(219)` in `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.

## Regression risk

Medium

The new provider is isolated, but the kernel identity issue is in the active data snapshot boundary that future high-precision factory and engine code will rely on for correct kernel selection and status reporting.

## Out-of-scope observations

- The full suite still has the recurring QML main-window footer popup toolbar failure tracked outside HP-023.
- Some provider tests use a fake runtime failure to represent a wrong kernel file rather than exercising a real CALCEPH rejection path; this is acceptable for unit scope but should be complemented by an integration fixture when real kernel test data is introduced.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
