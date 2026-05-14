## Task
- ID: HP-042
- Title: Extend scene and app cache keys for high precision

## Status
READY

## Acceptance criteria claimed
- [x] Scene snapshot cache key includes engine kind and options revision
- [x] Scene cache key includes ephemeris, EOP, and leap-second revisions
- [x] Scene cache key keeps astronomical epoch and observer/refraction inputs
- [x] Render-only projection/theme/view changes avoid ephemeris recomputation
- [x] Focused scene pipeline and controller tests pass
- [x] Full `build-ralph` test suite passes

## Files changed
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
- `apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp`
- `apps/skygate-ui/src/scene/SkySceneFramePipeline.hpp`
- `apps/skygate-ui/src/scene/SkySceneModel.cpp`
- `apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-042/implementation.md`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 123/125 tests; the
  two CALCEPH-dependent tests were skipped by the existing configuration.
