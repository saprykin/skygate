## Task fixed
  - ID: HP-022F
  - Title: Add update-flow test harness and fault injection
  - Source: IMPLEMENTATION_PLAN.md, high-precision ephemeris engine spec

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-022F/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-022F/implementation.md

## Summary
  Fixed the two MAJOR review findings. The update-flow harness now writes its
  source payload outside `Q_ASSERT`, and it includes a non-cancelled activation
  I/O failure case that preserves active data after verification succeeds.

## Findings addressed
  - Finding title: Source payload write is hidden behind Q_ASSERT
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Replaced the `Q_ASSERT(writeFile(...))` expression with an
    unconditional write and `QTest::qFail` on failure.
  - Why this resolves the finding: The source payload write now executes in
    debug and non-debug Qt builds, so the harness no longer depends on
    assertion compilation mode.

  - Finding title: Harness does not cover interrupted install failure
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Added
    `updateFlowHarnessInjectsActivationFailureAndPreservesActiveData()`, which
    blocks the activation target directory after staging is verified.
  - Why this resolves the finding: The new harness test exercises a
    non-cancelled activation failure after successful verification and asserts
    that active data, settings, revision, failed cache cleanup, and staging
    retention remain correct.

## Tests run
  - Command: `clang-format -i`
    `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
    Result: PASS
  - Command: `cmake --build build-ralph --target`
    `skygate-ui-sky-ephemeris-data-manager-tests`
    Result: PASS
  - Command: `ctest --test-dir build-ralph --output-on-failure -R`
    `'^skygate-ui-sky-ephemeris-data-manager-tests$'`
    Result: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-022F/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-022F/fix.md

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
