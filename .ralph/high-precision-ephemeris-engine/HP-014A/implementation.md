## Task
- ID: HP-014A
- Title: Implement TT and TDB conversion policy

## Status
READY

## Acceptance criteria claimed
- [x] `TimeScaleService` converts TT to TDB with a documented TT/TDB offset policy
- [x] `TimeScaleService` converts TDB back to TT with iterative offset solving
- [x] UTC and TAI requests can route to TDB through existing TT conversion
- [x] TT/TDB conversion preserves two-part Julian date precision through round trips
- [x] Conversion results expose a warning when TT/TDB approximation is applied
- [x] ERFA wrapper exposes `eraDtdb` when high precision is enabled, with a simple-only fallback approximation when it is not
- [x] Focused time-scale conversion tests added

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/include/skygate/ephemeris/TimeScaleService.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
- `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`

## Important notes
- `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so verification covered the contained fallback approximation path rather than a linked ERFA build.
- Verification run: `cmake --build build-ralph --target skygate-ephemeris-time-scale-service-tests` - PASS
- Verification run: `ctest --test-dir build-ralph -R skygate-ephemeris-time-scale-service-tests --output-on-failure` - PASS
- Verification run: `cmake --build build-ralph` - PASS
- Verification run: `ctest --test-dir build-ralph --output-on-failure` - 110/111 PASS; the only failure was the pre-existing `skygate-ui-qml-main-window-tests` footer toolbar toggle regression already tracked by HP-053.
- Verification run: `ctest --test-dir build-ralph -E '^skygate-ui-qml-main-window-tests$' --output-on-failure` - PASS
