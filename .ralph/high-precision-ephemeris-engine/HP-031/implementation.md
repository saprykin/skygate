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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Stellar parallax flag is ignored during propagation
  - Action: Fixed
  - Notes: Distance-based propagation and radial-velocity application now
    require `StellarParallax` to be requested with a positive parallax.
  - Finding title: Catalog RA proper motion is likely scaled by cos(dec) twice
  - Action: Fixed
  - Notes: The public astrometry payload now documents HYG-style tangent-plane
    `pmra` semantics and the calculator no longer applies an extra cos(dec).
  - Finding title: HYG missing-distance sentinel becomes valid parallax
  - Action: Fixed
  - Notes: The HYG parser now treats `dist` values at the 100000 pc sentinel
    as absent when deriving parallax.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/catalog/hyg/HygCatalogParser.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/tests/catalog/hyg/HygCatalogParserTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-031/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-031/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-catalog-hyg-tests`: FAIL, target name
    `skygate-ephemeris-catalog-hyg-tests` does not exist.
  - `cmake --build build-ralph --target
    skygate-ephemeris-hyg-catalog-tests`: PASS.
  - `ctest --test-dir build-ralph --output-on-failure -R`
    `'skygate-ephemeris-(star-astrometry-calculator|hyg-catalog|api-model)-tests'`:
    PASS, 3 tests passed.
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 122 tests
    passed with CALCEPH-only tests skipped in the current build.
- Remaining concerns: None.
