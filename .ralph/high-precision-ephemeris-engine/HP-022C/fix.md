## Task fixed
  - ID: HP-022C
  - Title: Verify staged update sets before activation
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-022C/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-022C/implementation.md

## Summary
  Staged update verification now lets callers require exact component versions and validity coverage before activation. The verifier rejects mismatched versions, insufficient validity ranges, and incomplete validity-range labels before accepting a staged set.

## Findings addressed
  - Finding title: Expected versions and validity ranges are not verified
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp; libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp; libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp
  - What changed: Added optional expected version and required validity range fields to expected staged components, added `MismatchedMetadata`, compared staged manifest metadata against the caller requirements, and added version/range mismatch regression tests.
  - Why this resolves the finding: A complete and checksummed staged set is no longer accepted when it is stale or does not cover the requested validity interval.

  - Finding title: Standalone metadata validation accepts incomplete validity-range labels
  - Severity: MINOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp; libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp
  - What changed: Required non-empty validity range id and display name during standalone staged metadata validation and added a regression test for missing label metadata.
  - Why this resolves the finding: Materialized manifests assembled outside the JSON parser now receive the same required validity-range label checks during staged verification.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests -j 2`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-data-activation-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 119/120 passed; the only failure was the known unrelated `skygate-ui-qml-main-window-tests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` case.

## Files changed
  - libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp
  - libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp
  - libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-022C/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-022C/fix.md

## Remaining concerns
  The recurring unrelated QML footer popup toolbar test failure remains outside HP-022C.

## Final fixer status
  READY_FOR_REVIEW
