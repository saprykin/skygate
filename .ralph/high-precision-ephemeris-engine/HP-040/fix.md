## Task fixed
  - ID: HP-040
  - Title: Decouple engine ownership from catalog rebuilds
  - Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-040/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-040/implementation.md

## Summary
  Fixed the controller rebuild path so high-precision engine ownership can carry
  the complete factory dependency bundle outside catalog rebuilds, and so an
  existing high-precision engine is not silently overwritten by a simple
  fallback when a rebuild cannot construct high precision.

## Findings addressed
  - Finding title: High-precision engine selection is downgraded during controller rebuild
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/src/app/SkyContextController.cpp; apps/skygate-ui/src/app/SkyContextController.hpp; apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Added controller initialization wiring for high-precision factory inputs and passed those inputs into every engine rebuild request. The controller now respects the configured fallback option and retains an already-active high-precision engine instead of replacing it with a simple fallback when high-precision rebuild inputs are incomplete. Added regression tests for preserving selected high-precision kind/options across catalog rebuilds and active ephemeris data changes.
  - Why this resolves the finding: The controller no longer builds high-precision requests with only kind/options/catalog/snapshot, and missing dependencies no longer cause an injected or selected high-precision engine to be silently downgraded during construction, catalog changes, or active data changes.

## Tests run
  - `cmake --build build-ralph --target skygate-ui-sky-ephemeris-data-manager-tests -j2`: PASS
  - `ctest --test-dir build-ralph -R 'skygate-ui-(sky-catalog-runtime|sky-ephemeris-data-manager|active-catalog-builder)-tests' --output-on-failure`: PASS
  - `cmake --build build-ralph -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL. 119/120 tests passed; the only failure was the known out-of-scope HP-057 `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Files changed
  - apps/skygate-ui/src/app/SkyContextController.cpp
  - apps/skygate-ui/src/app/SkyContextController.hpp
  - apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-040/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-040/fix.md

## Remaining concerns
  The HP-057 QML main-window failure remains out of scope for HP-040.

## Final fixer status
  READY_FOR_REVIEW
