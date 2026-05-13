## Task fixed
  - ID: HP-027G
  - Title: Add apparent RA/Dec Horizons validation
  - Source: IMPLEMENTATION_PLAN.md, specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-027G/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-027G/implementation.md

## Summary
  Restricted the new true-equator/equinox-of-date frame to the explicitly implemented GCRS-like direct transforms so it cannot flow through CIRS terrestrial stages with incorrect origin semantics. The numeric Horizons validation skip remains an environment limitation of the current simple-only build and is documented here and in the implementation handoff.

## Findings addressed
  - Finding title: True-equator/equinox frame is allowed through CIRS terrestrial stages
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp, libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp
  - What changed: Added an explicit `TrueEquatorAndEquinox` unsupported-transform guard after the direct GCRS-like transform handler and before the composed rank pipeline. Added regression coverage for `TrueEquatorAndEquinox` to/from TIRS and ITRS.
  - Why this resolves the finding: Unsupported terrestrial transforms now fail with `CorrectionUnavailable` and no applied corrections instead of returning valid-looking Earth-rotation results with CIRS semantics.

  - Finding title: Numeric Horizons validation is skipped in the available build
  - Severity: QUESTION
  - Action: Not applicable
  - File(s): .ralph/high-precision-ephemeris-engine/HP-027G/implementation.md, .ralph/high-precision-ephemeris-engine/HP-027G/fix.md
  - What changed: Documented that the current `build-ralph` tree has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` and `calceph_DIR-NOTFOUND`, so the ERFA-backed numeric assertion is expected to skip locally.
  - Why this resolves the finding: The test behavior is intentional for simple-only builds; the numeric comparison must be exercised in a high-precision-enabled build where ERFA/CALCEPH are available.

## Tests run
  - `clang-format -i libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-radec-validation-tests skygate-ephemeris-apparent-place-calculator-tests`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-frame-transformer-tests`: NOT RUN; target is not generated because `build-ralph` has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(apparent-radec-validation|apparent-place-calculator|frame-transformer|fixture-support)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-fixture-support-tests'`: PASS
  - `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-apparent-radec-validation-tests -v2`: PASS with one expected skip because ERFA-backed apparent RA/Dec validation is unavailable in this build.
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp
  - libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-027G/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-027G/fix.md

## Remaining concerns
  - The new frame-transformer regression test could not be executed in the current `build-ralph` tree because high precision is disabled and `skygate-ephemeris-frame-transformer-tests` is not generated.
  - The Horizons numeric tolerance assertion remains unexercised locally for the same high-precision dependency limitation.

## Final fixer status
  - READY_FOR_REVIEW
