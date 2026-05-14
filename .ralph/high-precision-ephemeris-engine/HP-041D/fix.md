## Task fixed
  - ID: HP-041D
  - Title: Apply selected engine and request to inspector and observation
    events
  - Source: IMPLEMENTATION_PLAN.md, specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-041D/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-041D/implementation.md

## Summary
  Fixed the context-based observation event overloads so their compatibility
  requests preserve the selected engine options. This keeps remaining
  context-only callers aligned with the engine's configured correction,
  atmosphere, fallback, and engine-kind behavior.

## Findings addressed
  - Finding title: Context event overloads drop engine options
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/src/ObservationEventCalculator.cpp,
    libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp
  - What changed: Synthesized context requests now copy
    `ephemerisEngine.options()`. Added a regression test proving the context
    overload receives engine-configured correction flags instead of default
    request flags.
  - Why this resolves the finding: Context overloads once again preserve the
    selected engine's compatibility options while still using the
    request-based sampling path added by HP-041D.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ephemeris-observation-event-calculator-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ephemeris-observation-event-calculator-tests`: PASS
  - `cmake --build build-ralph --target
    skygate-ui-sky-selection-overlay-builder-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ui-sky-selection-overlay-builder-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - libs/skygate-ephemeris/src/ObservationEventCalculator.cpp
  - libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-041D/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-041D/fix.md

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
