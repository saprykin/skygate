## Task fixed
  - ID: HP-031
  - Title: Implement single-star astrometry propagation
  - Source: `IMPLEMENTATION_PLAN.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-031/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-031/implementation.md`

## Summary
  Fixed three HP-031 numerical and parser issues from review. Stellar parallax
  is now honored as an explicit gate for distance-based propagation and radial
  velocity, RA proper motion uses catalog tangent-plane `pmra` semantics, and
  HYG missing-distance sentinels no longer become valid parallax.

## Findings addressed
  - Finding title: Stellar parallax flag is ignored during propagation
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - What changed:
    Distance-based propagation now requires the `StellarParallax` correction
    flag and positive parallax. Radial velocity is marked unavailable unless
    that enabled parallax is present.
  - Why this resolves the finding:
    Requests that omit `StellarParallax` no longer use parallax-dependent
    distance data or report radial velocity as applied.

  - Finding title: Catalog RA proper motion is likely scaled by cos(dec) twice
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - What changed:
    `CatalogStarAstrometry` documents RA proper motion as tangent-plane
    `mu_alpha * cos(delta)`, and the calculator no longer multiplies it by
    `cos(delta)` a second time. Tests now cover non-equatorial `pmra`.
  - Why this resolves the finding:
    HYG-style `pmra` is consumed with matching semantics, avoiding systematic
    under-propagation away from the equator.

  - Finding title: HYG missing-distance sentinel becomes valid parallax
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/catalog/hyg/HygCatalogParser.cpp`,
    `libs/skygate-ephemeris/tests/catalog/hyg/HygCatalogParserTests.cpp`
  - What changed:
    The HYG parser treats derived parallax from `dist >= 100000` pc as absent.
    Parser coverage verifies the sentinel still preserves other astrometry
    fields while leaving `stellarParallaxMas` empty.
  - Why this resolves the finding:
    Missing HYG distance data can no longer enable stellar parallax or radial
    velocity availability through a sentinel-derived positive parallax.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-catalog-hyg-tests`: FAIL. The guessed HYG target name
    does not exist; this was replaced by the correct target below.
  - `cmake --build build-ralph --target
    skygate-ephemeris-hyg-catalog-tests`: PASS.
  - `ctest --test-dir build-ralph --output-on-failure -R`
    `'skygate-ephemeris-(star-astrometry-calculator|hyg-catalog|api-model)-tests'`:
    PASS. 3 tests passed.
  - `ctest --test-dir build-ralph --output-on-failure`: PASS. 122 tests
    passed, with the two CALCEPH-only tests skipped in this build.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-031/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-031/implementation.md`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/catalog/hyg/HygCatalogParser.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/tests/catalog/hyg/HygCatalogParserTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
