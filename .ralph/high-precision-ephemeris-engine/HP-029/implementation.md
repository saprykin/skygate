## Task
- ID: HP-029
- Title: Implement atmospheric refraction calculation

## Status
READY

## Acceptance criteria claimed
- [x] `AtmosphericRefractionCalculator` added
- [x] Refraction applies only when requested and observer/atmosphere inputs are
  valid
- [x] Refraction disabled behavior leaves results unchanged
- [x] Missing horizontal, invalid observer, invalid atmosphere, and out-of-range
  altitude report degraded correction-unavailable metadata
- [x] High-precision factory wires the refraction calculator into the apparent
  place pipeline
- [x] Focused ephemeris tests pass

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/AtmosphericRefractionCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/AtmosphericRefractionCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`

## Important notes
- Verification run: `cmake -S . -B build-ralph`
- Verification run: `cmake --build build-ralph --target
  skygate-ephemeris-atmospheric-refraction-calculator-tests
  skygate-ephemeris-apparent-place-calculator-tests`
- Verification run: focused CTest filter for
  `skygate-ephemeris-atmospheric-refraction-calculator-tests`,
  `skygate-ephemeris-apparent-place-calculator-tests`,
  `skygate-ephemeris-highprecision-engine-tests`, and
  `skygate-ephemeris-engine-factory-behavior-tests`
- Full-suite verification run: `ctest --test-dir build-ralph
  --output-on-failure`
- Full-suite result: 120/121 tests passed, with only unrelated
  `skygate-ui-qml-main-window-rendering-tests` failing because a
  `SkyOverlayLabel` was above the viewport. This was recorded as HP-059 in
  `IMPLEMENTATION_PLAN.md`.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Refraction input validation coverage is incomplete
  - Action: Fixed
  - Notes: Added focused coverage for invalid observer input and each
    atmospheric pressure, temperature, relative-humidity, and wavelength
    validation branch. The tests assert unchanged altitude, degraded metadata,
    `CorrectionUnavailable`, and no applied atmospheric-refraction flag. Added
    boundary-altitude coverage for the accepted -1 degree boundary, high
    accepted altitude clamping behavior, and rejected zenith input.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-029/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-029/fix.md`
- Tests run after fix:
  - `clang-format -i
    libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`
  - `cmake --build build-ralph --target
    skygate-ephemeris-atmospheric-refraction-calculator-tests
    skygate-ephemeris-apparent-place-calculator-tests
    skygate-ephemeris-highprecision-engine-tests
    skygate-ephemeris-apparent-radec-validation-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    '^(skygate-ephemeris-atmospheric-refraction-calculator-tests|skygate-ephemeris-apparent-place-calculator-tests|skygate-ephemeris-highprecision-engine-tests|skygate-ephemeris-apparent-radec-validation-tests)$'`
    - PASS after correcting an over-strict high-altitude assertion
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 121/121 tests
    passed with CALCEPH-gated tests skipped in this build configuration
- Remaining concerns: None.
