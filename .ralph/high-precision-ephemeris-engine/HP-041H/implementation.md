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
