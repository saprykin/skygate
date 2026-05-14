## Task
- ID: HP-041B
- Title: Apply selected engine and request to frame rendering

## Status
READY

## Acceptance criteria claimed
- [x] Frame rendering uses the selected engine through the request-based API
- [x] Scene input is populated from the app-level ephemeris request context
- [x] Integration coverage proves selected engine kind and request options
      affect rendered snapshot coordinates
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/scene/SkySceneFramePipeline.hpp`
- `apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp`
- `apps/skygate-ui/src/scene/SkySceneModel.cpp`
- `apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-041B/implementation.md`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 124/124 configured
  tests. The CALCEPH kernel provider and solar-system state calculator tests
  were skipped by the existing build configuration.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Request option changes can reuse stale frame snapshots
  - Action: Fixed
  - Notes: The frame snapshot cache key now includes request epoch and engine
    options whenever request-based computation is used. Request-only changes on
    the same engine instance now recompute the snapshot and rebuild the render
    frame through the existing snapshot generation dependency.
- Files changed during fix pass:
  - `apps/skygate-ui/src/scene/SkySceneFramePipeline.hpp`
  - `apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp`
  - `apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041B/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041B/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ui-sky-scene-frame-pipeline-tests` PASS
  - `ctest --test-dir build-ralph -R skygate-ui-sky-scene-frame-pipeline-tests
    --output-on-failure` PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS
- Remaining concerns: None.
