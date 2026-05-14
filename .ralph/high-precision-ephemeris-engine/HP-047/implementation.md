## Task
- ID: HP-047
- Title: Validate clean-install offline behavior and optional DE441 flow

## Status
READY

## Acceptance criteria claimed
- [x] Clean-install bundled fallback starts without installed ephemeris data
- [x] Offline modern ephemeris data activation is covered by acceptance tests
- [x] Absent DE441 state remains visible as not installed
- [x] Optional DE441 profile activation exposes the long-range kernel
- [x] Clearing ephemeris data returns to bundled fallback behavior
- [x] Existing full `build-ralph` test suite passes

## Files changed
- `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 127/127 tests.
- `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests` were skipped because
  the existing `build-ralph` tree has
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
