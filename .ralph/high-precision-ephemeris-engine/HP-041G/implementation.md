## Task
- ID: HP-041G
- Title: Reconcile observer/time-dependent reference overlays

## Status
READY

## Acceptance criteria claimed
- [x] Scene-graph reference lines use the resolved scene snapshot context
- [x] Reference overlay labels use the selected request context
- [x] Pure reference overlays remain independent of ephemeris engine body lookup
- [x] Tests document resolved-context and pure-reference overlay behavior
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/render/SkyViewportItem.cpp`
- `apps/skygate-ui/src/scene/SkySceneModel.cpp`
- `apps/skygate-ui/src/scene/SkySceneModel.hpp`
- `apps/skygate-ui/tests/scene/SkySceneCompositionTests.cpp`
- `apps/skygate-ui/tests/scene/SkySceneModelFrameTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed: 124 tests,
  with the existing CALCEPH-dependent tests skipped by their configured skip
  conditions.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Reference labels ignore resolved snapshot context
  - Action: Fixed
  - Notes: `SkySceneComposer::buildOverlayItems()` now resolves reference label
    context from the frame pipeline snapshot before falling back to the scene
    frame snapshot or input context. A regression test covers a pipeline
    snapshot context that differs from the input context.
- Files changed during fix pass:
  - `apps/skygate-ui/src/scene/SkySceneComposition.cpp`
  - `apps/skygate-ui/src/scene/SkySceneComposition.hpp`
  - `apps/skygate-ui/tests/scene/SkySceneCompositionTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041G/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041G/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ui-sky-scene-composition-tests`
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-(sky-scene-composition|scene-model-frame)-tests'`
  - `ctest --test-dir build-ralph --output-on-failure`
- Remaining concerns: None.
