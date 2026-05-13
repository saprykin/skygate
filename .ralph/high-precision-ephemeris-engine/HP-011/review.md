## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-011
- Title: Add leap-second table loading
- Source: Spec: Time Model; Data Management; Testing Requirements
- Base ref: 1c286ef
- Head ref: e746ffb

## Summary

The implementation adds a public `ILeapSecondProvider`, a table-backed provider, snapshot/text-asset loading, metadata exposure, and focused Qt tests. The core path builds and the relevant tests pass, but malformed expiration metadata is silently ignored even though expiration drives stale status and validity-range reporting. That leaves an acceptance-criteria gap for malformed table handling and stale-status reliability.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Invalid expiration metadata is accepted as a usable table

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/LeapSecondProvider.cpp`
Lines/functions: `applyMetadataLine`, lines 117-121; `loadLeapSecondTableFromTextAsset`, lines 290-297

Problem:
`#@ expires ...` is the only parsed metadata that establishes expiration and stale-table behavior, but an invalid expiration date is silently ignored. The loader still returns `Available`, leaves `expiresAt` unset, skips stale detection, and falls back to a validity range ending at the last leap-second row.

Why it matters:
HP-011 requires malformed table handling, stale table status, and validity-range metadata. A table with malformed expiration metadata can no longer report staleness correctly, and the reported validity range becomes misleading while the load result still appears successful.

Recommended fix:
Treat a recognized `#@ expires` line with an unparsable date as `Malformed` and add test coverage for malformed metadata. If expiration metadata is optional by design, return an explicit diagnostic/status that does not claim normal availability and does not substitute the last data row as the freshness boundary.

## Test assessment

Added tests cover valid table loading, missing table, malformed data rows, stale table detection with a valid expiration date, and validity-range metadata. I ran `cmake --build build-ralph --target skygate-ephemeris-leap-second-provider-tests`, `ctest --test-dir build-ralph -R skygate-ephemeris-leap-second-provider-tests --output-on-failure`, `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|leap-second-provider)-tests" --output-on-failure`, and the full `ctest --test-dir build-ralph --output-on-failure`; all passed. Missing coverage: malformed expiration metadata, which is part of the metadata used for stale/validity behavior.

## Regression risk

Medium

The implementation is isolated and existing tests pass, but accepting bad freshness metadata can silently compromise later time-scale conversion and data-status reporting.

## Out-of-scope observations

- The prompt references `specs/high-precision-ephemeris-engine.md`, but the repository contains `spec/high-precision-ephemeris-engine.md`; I reviewed the available spec file.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
