## Task fixed
  - ID: HP-041H
  - Title: Add selected-engine integration test matrix
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-041H/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-041H/implementation.md`

## Summary
  Strengthened the selected-engine matrix so it proves each reviewed consumer
  changes with the fake engine tier or correction options, rather than only
  proving that generic payloads are non-empty.

## Findings addressed
  - Finding title: Matrix does not prove several consumers switch
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp
  - What changed: Added consumer observation fingerprints for target render
    point coordinates, inspector coordinate/event fields, trail line geometry,
    and night-condition payload fields. The test now compares those
    fingerprints across Simple, HighPrecision without corrections, and
    HighPrecision with LightTime.
  - Why this resolves the finding: The matrix now fails if render, events,
    trails, or night conditions continue using stale/simple data while the
    selected engine kind or correction flags change.

  - Finding title: Applicable reference overlays are not exercised
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp
  - What changed: Enabled ecliptic, celestial-equator, and circumpolar
    reference overlay layers in the matrix harness, then asserted that their
    `referenceLine` labels are emitted and their label positions differ across
    selected engine/options tiers.
  - Why this resolves the finding: The matrix now exercises the actual
    UI-facing reference overlay composition path using the selected snapshot
    context, not only the cached context accessor.

## Tests run
  - `clang-format -i`
    apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp:
    PASS
  - `cmake --build build-ralph --target
    skygate-ui-context-controller-selected-engine-matrix-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ui-context-controller-selected-engine-matrix-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 125/125 tests
    passed with the two configured CALCEPH-dependent tests skipped

## Files changed
  - apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp
  - `.ralph/high-precision-ephemeris-engine/HP-041H/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041H/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
