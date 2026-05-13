## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-013
- Title: Add Delta T data/model provider
- Source: `IMPLEMENTATION_PLAN.md`, `spec/high-precision-ephemeris-engine.md`
- Base ref: 86c7012b95f552962e05314bac3a2b54f695d486
- Head ref: ac4f240cdf00ca3db6278ec27215fb05e5de1176

## Summary

The implementation adds a public Delta T provider API, table-backed text loader, snapshot integration, metadata exposure, and Qt tests. The general shape matches the task, and the registered tests pass, but the provider currently advertises a validity interval that it cannot actually serve and accepts fallback metadata that can produce a usable estimate with no Delta T value. Those are correctness issues for the provider contract, so this needs another implementation pass.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Validity range can exceed usable table coverage

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
Lines/functions: lines 327-376, 471-472; `TableBackedDeltaTProvider::deltaTSeconds`, `loadDeltaTDataFromTextAsset`

Problem:
`loadDeltaTDataFromTextAsset` sets `dataInfo.validityRange.end` to `expiresAt` when the metadata exists, but `deltaTSeconds()` only returns table-backed estimates through `m_entries.back()`. With the current fixture shape, metadata reports validity through `2027-01-01` while the last table row is `2026-01-01`; a request such as `2026-06-01` is inside the advertised validity range but returns `Unavailable`.

Why it matters:
HP-013 requires validity-range reporting and degradation state. Callers will use this metadata to decide whether Delta T data can satisfy a request; advertising an interval the provider cannot serve causes false coverage and can incorrectly drive HP-014 time conversion behavior.

Recommended fix:
Either report the provider validity range only through the last usable table row, or implement a defined future/prediction policy through `expiresAt` and return a degraded estimate with clear provenance and diagnostics. Add a test that requests an epoch between the last row and `expiresAt`.

### Finding 2: Ancient fallback can be marked usable without a Delta T value

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
Lines/functions: lines 364-373, 452-468; `TableBackedDeltaTProvider::deltaTSeconds`, `loadDeltaTDataFromTextAsset`

Problem:
When ancient fallback range metadata exists, `deltaTSeconds()` returns `DeltaTEstimateStatus::Degraded` even if `representativeDeltaTSeconds` was never provided. `DeltaTEstimate::isUsable()` treats `Degraded` as usable, so callers can receive a usable estimate with `deltaTSeconds == std::nullopt`.

Why it matters:
The provider's estimate contract becomes ambiguous exactly in the degraded ancient-date path HP-013 is meant to expose. Downstream time conversion cannot apply Delta T if the estimate is marked usable but contains no value.

Recommended fix:
Require `ancient_fallback_delta_t_seconds` when accepting fallback metadata that should produce estimates, or return `Unavailable` when the fallback has no representative estimate. Add a malformed or unavailable-path test for fallback metadata without a representative value.

### Finding 3: Partial fallback ranges can fabricate missing endpoints

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
Lines/functions: lines 142-170, 452-460; `setFallbackStart`, `setFallbackEnd`, `loadDeltaTDataFromTextAsset`

Problem:
The loader validates fallback range endpoints by checking whether `validityRange.start` and `validityRange.end` are finite. Those fields default to finite Julian date zero, so metadata with only `ancient_fallback_start` or only `ancient_fallback_end` can pass validation and silently invent the missing endpoint.

Why it matters:
This can expose incorrect ancient fallback coverage metadata and return degraded estimates for dates the data file did not actually declare. It weakens the validity-range guarantees required by this task.

Recommended fix:
Track whether fallback start and end metadata were explicitly provided, then reject partial fallback ranges as malformed. Add tests for start-only and end-only fallback metadata.

## Test assessment

`skygate-ephemeris-delta-t-provider-tests` covers present data, missing data, malformed rows, ancient fallback metadata, validity metadata, and stale data. The target is registered in CMake and runs in the existing `build-ralph` tree.

Gaps remain around the failing behaviors above: there is no test for an epoch inside advertised `expiresAt` coverage but beyond the last table row, no test for fallback metadata without a representative Delta T value, and no test for partial fallback ranges. Some metadata assertions are also weak: fallback numeric values and complete split-epoch fields are not compared directly.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-delta-t-provider-tests`
- `ctest --test-dir build-ralph -R skygate-ephemeris-delta-t-provider-tests --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

All 53 tests in the current `build-ralph` configuration passed.

## Regression risk

Medium

The code is mostly self-contained, but Delta T validity and fallback semantics are foundational for the next time-scale conversion tasks. Incorrect metadata can propagate into conversion policy and UI/status reporting.

## Out-of-scope observations

- `deltaTSeconds()` accepts any finite `AstronomicalEpoch` time scale even though table rows are parsed as UTC dates. This should be clarified before HP-014 wires Delta T into time conversion, but it is less urgent than the current coverage and fallback contract issues.
- Test assertions for fallback numeric metadata and full epoch equality could be tightened in the fix pass.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
