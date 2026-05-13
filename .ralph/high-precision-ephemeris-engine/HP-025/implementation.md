## Task
- ID: HP-025
- Title: Implement geometric solar-system vector calculation

## Status
READY

## Acceptance criteria claimed
- [x] Added `SolarSystemStateCalculator` under the high-precision engine boundary.
- [x] CALCEPH kernel provider can return geocentric geometric position vectors in AU.
- [x] Supported major solar-system body IDs map to NAIF target IDs.
- [x] Geometric vectors are converted to request-path RA/Dec without apparent corrections.
- [x] Unsupported body, missing kernel provider, out-of-range kernel status, and non-TDB epoch cases return structured status/warnings.
- [x] Added deterministic coverage including a compact JPL Horizons geometric vector smoke fixture.
- [x] Relevant ephemeris tests pass.

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` still fails only in unrelated `skygate-ui-qml-main-window-tests`, `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`. This is tracked separately as `HP-056`.

## Verification
- `clang-format -i` on touched C++ headers/sources.
- `cmake -S . -B build-ralph`
- `cmake --build build-ralph --target skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|engine-baseline|engine-fallback|regression)-tests'`
- `cmake --build build-ralph`
- `ctest --test-dir build-ralph --output-on-failure` (115/116 passed; unrelated `HP-056` failure remains)
