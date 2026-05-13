## Task fixed
  - ID: HP-009
  - Title: Add the high-precision engine facade boundary
  - Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-009/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-009/implementation.md

## Summary
  Fixed the HP-009 review findings by strengthening the option-forwarding test so it proves per-request correction flags override engine defaults, and by registering the facade test with the `highprecision` CTest label.

## Findings addressed
  - Finding title: Option-forwarding test does not distinguish request options from engine defaults
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp
  - What changed: The test now constructs `HighPrecisionEphemerisEngine` with `AtmosphericRefraction` as the engine default while the request uses `LightTime | EarthOrientation`, then keeps assertions on the calculator, apparent-place calculator, result builder, and returned metadata flags.
  - Why this resolves the finding: The test would now fail if the facade forwarded constructor defaults instead of the per-request options.

  - Finding title: High-precision facade tests are not discoverable through the highprecision label
  - Severity: MINOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/tests/CMakeLists.txt
  - What changed: Added `LABELS "unit;highprecision"` to `skygate-ephemeris-highprecision-engine-tests`.
  - Why this resolves the finding: `ctest -L highprecision -N` now discovers the HP-009 facade test.

## Tests run
  - `clang-format -i libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision-engine-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -L highprecision -N`: PASS
  - `ctest --test-dir build-ralph -L highprecision --output-on-failure`: PASS
  - `git diff --check`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 106/107 passed; `skygate-ui-qml-main-window-tests` still fails in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` and is tracked separately as HP-053.

## Files changed
  - .ralph/high-precision-ephemeris-engine/HP-009/fix.md
  - .ralph/high-precision-ephemeris-engine/HP-009/implementation.md
  - libs/skygate-ephemeris/tests/CMakeLists.txt
  - libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp

## Remaining concerns
  The unrelated UI test failure tracked as HP-053 remains outside HP-009.

## Final fixer status
  READY_FOR_REVIEW
