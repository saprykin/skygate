## Task
- ID: HP-001
- Title: Move the existing simple engine into the required source layout

## Status
READY

## Acceptance criteria claimed
- [x] Existing `SimpleEphemerisEngine` moved under `libs/skygate-ephemeris/src/engine/simple/`
- [x] Simple-only approximate engine calculators moved under `libs/skygate-ephemeris/src/engine/simple/`
- [x] High-precision source layout placeholder added under `libs/skygate-ephemeris/src/engine/highprecision/`
- [x] `libs/skygate-ephemeris/CMakeLists.txt` updated without changing simple-engine behavior
- [x] `clang-format` run on touched C++ source and header files
- [x] `skygate-ephemeris` builds in `build-ralph`
- [x] Required simple-engine tests pass
- [x] Full UI-off test suite in `build-ralph` passes

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/CelestialReferenceCalculator.cpp`
- `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/.gitkeep`
- `libs/skygate-ephemeris/src/engine/simple/AstronomicalTime.cpp`
- `libs/skygate-ephemeris/src/engine/simple/AstronomicalTime.hpp`
- `libs/skygate-ephemeris/src/engine/simple/EclipticToEquatorialCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/EclipticToEquatorialCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/simple/EquatorialToHorizontalCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/EquatorialToHorizontalCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/simple/MoonEquatorialCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/MoonEquatorialCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/simple/PlanetEquatorialCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/PlanetEquatorialCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SunEquatorialCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SunEquatorialCalculator.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisRegressionTests.cpp`
- `libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp`

## Important notes
- Verification commands run:
  `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug`
- Verification commands run:
  `cmake --build build-ralph --target skygate-ephemeris -j2`
- Verification commands run:
  `cmake --build build-ralph --target skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests -j2`
- Verification commands run:
  `ctest --test-dir build-ralph -R 'skygate-ephemeris-(engine-baseline|engine-fallback|regression)-tests' --output-on-failure`
- Verification commands run:
  `cmake --build build-ralph -j2`
- Verification commands run:
  `ctest --test-dir build-ralph --output-on-failure`
