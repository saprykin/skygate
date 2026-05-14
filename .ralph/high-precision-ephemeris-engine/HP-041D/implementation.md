## Task
- ID: HP-041D
- Title: Apply selected engine and request to inspector and observation events

## Status
READY

## Acceptance criteria claimed
- [x] Object inspector uses request-based ephemeris context for
  observation events
- [x] Observation event calculations preserve selected engine options
- [x] Observation event sampling updates request epochs per sampled UTC time
- [x] Inspector/event tests cover request-option propagation
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/scene/SkySceneComposition.cpp`
- `apps/skygate-ui/src/selection/SkyObjectInspectorBuilder.cpp`
- `apps/skygate-ui/src/selection/SkySelectionOverlayBuilder.hpp`
- `apps/skygate-ui/tests/selection/SkySelectionOverlayBuilderTests.cpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/`
  `ObservationEventCalculator.hpp`
- `libs/skygate-ephemeris/src/ObservationEventCalculator.cpp`
- `libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed: 122 passed,
  2 skipped high-precision dependency tests.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Context event overloads drop engine options
  - Action: Fixed
  - Notes: Context-based observation event overloads now seed synthesized
    requests from `ephemerisEngine.options()`, preserving selected correction
    and atmosphere options for compatibility callers.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/ObservationEventCalculator.cpp`
  - `libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041D/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041D/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ephemeris-observation-event-calculator-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ephemeris-observation-event-calculator-tests`: PASS
  - `cmake --build build-ralph --target
    skygate-ui-sky-selection-overlay-builder-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    skygate-ui-sky-selection-overlay-builder-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: None.
