## Task
- ID: HP-032A
- Title: Add immutable cache-friendly catalog astrometry arrays

## Status
READY

## Acceptance criteria claimed
- [x] Immutable catalog-star astrometry array snapshot added
- [x] Full astrometry, partial astrometry, and fixed-only stars are covered
- [x] Snapshot copies source catalog data and survives source lifetime changes
- [x] Touched C++ files formatted with clang-format
- [x] Focused test target passes
- [x] Full `build-ralph` CTest suite passes

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`
- `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 123/123 tests.
- CALCEPH-dependent tests were skipped by the existing high-precision-disabled
  `build-ralph` configuration.
