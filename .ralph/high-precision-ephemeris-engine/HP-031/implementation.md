## Task
- ID: HP-031
- Title: Implement single-star astrometry propagation

## Status
READY

## Acceptance criteria claimed
- [x] `StarAstrometryCalculator` added for single-star astrometry propagation
- [x] Catalog star astrometry payload exposed on `CelestialBody`
- [x] Proper motion, stellar parallax, and radial velocity flags respected
- [x] Fixed-only and partial-astrometry fallback warnings covered
- [x] HYG astrometry columns parsed when available
- [x] Focused and full `build-ralph` tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/catalog/hyg/HygCatalogParser.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/catalog/hyg/HygCatalogParserTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Important notes
- `HP-031` found the public body model still lacked the astrometry payload
  needed by this task, so the implementation adds `CatalogStarAstrometry`.
- `ctest --test-dir build-ralph --output-on-failure` passed: 122 tests, 0
  failed. The existing CALCEPH-only tests were skipped because this build tree
  has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
