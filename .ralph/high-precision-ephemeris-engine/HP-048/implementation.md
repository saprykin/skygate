## Task
- ID: HP-048
- Title: Final acceptance test matrix

## Status
READY

## Acceptance criteria claimed
- [x] Final acceptance matrix test target added
- [x] Engine selection strict/fallback behavior covered
- [x] CALCEPH-provider-backed solar-system RA/Dec covered
- [x] Correction option application covered
- [x] Missing DE441-style long-range fallback warnings covered
- [x] Bundled modern and optional long-range data metadata covered
- [x] Deterministic Horizons fixture readability covered
- [x] Full-frame computation cache reuse covered
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/`
  `EphemerisAcceptanceMatrixTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed 126/126.
- CALCEPH-only kernel targets were skipped in this build configuration because
  high-precision dependencies are disabled.
