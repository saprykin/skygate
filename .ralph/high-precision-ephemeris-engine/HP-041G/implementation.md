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
