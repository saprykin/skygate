## Task fixed
  - ID: HP-032A
  - Title: Add immutable cache-friendly catalog astrometry arrays
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-032A/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-032A/implementation.md`

## Summary
  Fixed the catalog-star array snapshot so fixed-coordinate non-star objects do
  not enter the star-only layout. Exposed the stored immutable batch arrays as
  read-only spans and made numeric masks represent usable values rather than raw
  optional presence.

## Findings addressed
  - Finding title: Non-star fixed bodies enter star astrometry arrays
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
  - What changed: The inclusion predicate no longer admits every
    `FixedEquatorial` body. A regression test now verifies fixed-coordinate
    deep-sky objects are excluded while fixed-only stars are retained.
  - Why this resolves the finding: The snapshot remains star-only and will not
    feed fixed deep-sky rows into the future batch star path.

  - Finding title: Batch-needed astrometry arrays are not exposed as arrays
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.hpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
  - What changed: Added read-only span accessors for catalog/fallback masks,
    fixed fallback coordinates, proper motion arrays, stellar parallax arrays,
    radial velocity arrays, and validity ranges.
  - Why this resolves the finding: HP-032B can iterate cache-friendly value and
    mask arrays directly without per-index optional getters.

  - Finding title: Presence masks do not match scalar validity semantics
  - Severity: MINOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
  - What changed: Proper motion and radial velocity masks now require finite
    values. Stellar parallax masks now require finite positive values.
  - Why this resolves the finding: Batch consumers using these masks will see
    the same usable/unusable distinction as the scalar astrometry calculator.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-catalog-star-astrometry-arrays-tests`
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-catalog-star-astrometry-arrays-tests`
    PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS

## Files changed
  - `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/CatalogStarAstrometryArraysTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-032A/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-032A/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
