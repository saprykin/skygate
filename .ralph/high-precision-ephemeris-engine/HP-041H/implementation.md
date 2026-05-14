## Task
- ID: HP-041H
- Title: Add selected-engine integration test matrix

## Status
READY

## Acceptance criteria claimed
- [x] Matrix covers Simple and HighPrecision fake engines with distinct
  coordinates
- [x] Matrix verifies render, search focus, tracking, inspector/event fields,
  trails, night conditions, and reference overlay context use request APIs
- [x] Option changes are covered separately from catalog/search changes
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/tests/CMakeLists.txt`
- `apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed: 125/125
  tests passed. The existing CALCEPH kernel/provider tests were skipped by
  their configured skip rules.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Matrix does not prove several consumers switch
    - Action: Fixed
    - Notes: Strengthened matrix assertions to compare render point positions,
      inspector coordinate/event fields, trail geometry fingerprints, and
      night-condition payloads across simple, high-precision astrometric, and
      high-precision light-time tiers.
  - Finding title: Applicable reference overlays are not exercised
    - Action: Fixed
    - Notes: Enabled ecliptic, celestial-equator, and circumpolar reference
      overlay layers in the harness and asserted emitted `referenceLine`
      overlay labels and label positions from the selected snapshot context.
- Files changed during fix pass:
  - apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp
  - `.ralph/high-precision-ephemeris-engine/HP-041H/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041H/fix.md`
- Tests run after fix:
  - `clang-format -i
    apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp`
  - `cmake --build build-ralph --target
    skygate-ui-context-controller-selected-engine-matrix-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ui-context-controller-selected-engine-matrix-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 125/125 tests
    passed with the two configured CALCEPH-dependent tests skipped.
- Remaining concerns: None.
