## Task fixed
  - ID: HP-041G
  - Title: Reconcile observer/time-dependent reference overlays
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-041G/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-041G/implementation.md

## Summary
Reference overlay labels now use the resolved scene snapshot context from the
frame pipeline before falling back to the previous scene frame snapshot or input
context. This keeps label placement in the same observer/time frame as the
reference lines.

## Findings addressed
  - Finding title: Reference labels ignore resolved snapshot context
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/src/scene/SkySceneComposition.cpp`,
    `apps/skygate-ui/src/scene/SkySceneComposition.hpp`,
    `apps/skygate-ui/tests/scene/SkySceneCompositionTests.cpp`
  - What changed: `SkySceneComposer::buildOverlayItems()` now receives the
    frame pipeline result and resolves the label context from its snapshot when
    available. The scene composition tests now populate snapshot context for
    existing overlay label coverage and add a regression case where the input
    context differs from the resolved snapshot context.
  - Why this resolves the finding: Ecliptic, celestial-equator, and circumpolar
    label calculations share the resolved snapshot context, matching the
    scene-graph reference overlay context used for the lines.

## Tests run
  - `cmake --build build-ralph --target skygate-ui-sky-scene-composition-tests`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-(sky-scene-composition|scene-model-frame)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `apps/skygate-ui/src/scene/SkySceneComposition.cpp`
  - `apps/skygate-ui/src/scene/SkySceneComposition.hpp`
  - `apps/skygate-ui/tests/scene/SkySceneCompositionTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041G/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041G/fix.md`

## Remaining concerns
None.

## Final fixer status
READY_FOR_REVIEW
