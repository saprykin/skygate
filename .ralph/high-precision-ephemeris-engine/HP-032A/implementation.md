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

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Non-star fixed bodies enter star astrometry arrays
    - Action: Fixed
    - Notes: Restricted array inclusion to bodies with star type or source and
      added fixed deep-sky exclusion coverage.
  - Batch-needed astrometry arrays are not exposed as arrays
    - Action: Fixed
    - Notes: Added read-only span accessors for fallback coordinates,
      astrometry value arrays, masks, and validity ranges.
  - Presence masks do not match scalar validity semantics
    - Action: Fixed
    - Notes: Proper motion and radial velocity masks now require finite values;
      stellar parallax masks require finite positive values.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-032A/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-032A/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-catalog-star-astrometry-arrays-tests`
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-catalog-star-astrometry-arrays-tests`
    PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS
- Remaining concerns: None.
