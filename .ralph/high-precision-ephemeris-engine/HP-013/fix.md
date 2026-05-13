## Task fixed
  - ID: HP-013
  - Title: Add Delta T data/model provider
  - Source: `IMPLEMENTATION_PLAN.md`, `spec/high-precision-ephemeris-engine.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-013/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-013/implementation.md`

## Summary
  Fixed the Delta T provider contract so reported table validity matches actual estimate coverage, ancient fallback estimates require a representative Delta T value, and partial fallback ranges are rejected as malformed metadata.

## Findings addressed
  - Finding title: Validity range can exceed usable table coverage
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`, `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`
  - What changed: `validityRange.end` now uses the last table row instead of `expiresAt`; `expiresAt` remains available separately for stale-data checks. Added a regression test for a date between the last table row and expiration.
  - Why this resolves the finding: Callers no longer see provider coverage for dates that `deltaTSeconds()` cannot serve from the table.

  - Finding title: Ancient fallback can be marked usable without a Delta T value
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`, `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`
  - What changed: The loader now rejects ancient fallback metadata that lacks `ancient_fallback_delta_t_seconds`. Added a malformed-data regression test.
  - Why this resolves the finding: A degraded fallback estimate can no longer be exposed as usable without a concrete Delta T value.

  - Finding title: Partial fallback ranges can fabricate missing endpoints
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`, `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`
  - What changed: The parser tracks explicit fallback start and end metadata and rejects start-only or end-only fallback ranges. Added regression tests for both partial range shapes.
  - Why this resolves the finding: Default epoch values can no longer masquerade as declared ancient fallback range endpoints.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-delta-t-provider-tests`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-delta-t-provider-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-013/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-013/implementation.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
