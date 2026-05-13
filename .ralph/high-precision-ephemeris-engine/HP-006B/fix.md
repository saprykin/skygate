## Task fixed
  - ID: HP-006B
  - Title: Add request-based snapshot compute API
  - Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-006B/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-006B/implementation.md

## Summary
  The simple-engine request snapshot path now consumes request option fields explicitly. Unsupported correction or atmospheric refraction requests keep the existing simple-engine coordinate behavior but mark result metadata as degraded with a `CorrectionUnavailable` warning and `NoCorrections` applied.

## Findings addressed
  - Finding title: Request options are ignored by the simple-engine request path
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp; libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp
  - What changed: Added request option normalization/reporting for the simple engine and added test coverage for unsupported requested corrections/refraction.
  - Why this resolves the finding: Callers no longer receive apparently valid simple-engine request results when unsupported options were requested; the result metadata now states that requested corrections were unavailable.

## Tests run
  - clang-format -i libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp: PASS
  - cmake --build build-ralph --target skygate-ephemeris-engine-baseline-tests: PASS
  - ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure: PASS
  - cmake --build build-ralph: PASS
  - git diff --check: PASS
  - ctest --test-dir build-ralph --output-on-failure: FAIL, 102/103 passed; only the pre-existing HP-051 `skygate-ui-qml-main-window-tests` footer popup regression failed.

## Files changed
  - libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp
  - libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-006B/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-006B/fix.md

## Remaining concerns
  Full-suite verification still reports the pre-existing HP-051 QML failure outside HP-006B.

## Final fixer status
  READY_FOR_REVIEW
