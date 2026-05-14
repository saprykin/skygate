## Task fixed
  - ID: HP-041B
  - Title: Apply selected engine and request to frame rendering
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-041B/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-041B/implementation.md

## Summary
Fixed stale frame snapshots when request-only fields changed. The frame
snapshot cache now compares the request epoch and ephemeris engine options when
request-based rendering is active, so correction option, engine-kind, fallback,
refraction, and epoch changes recompute the snapshot for the same engine
instance.

## Findings addressed
  - Finding title: Request option changes can reuse stale frame snapshots
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    apps/skygate-ui/src/scene/SkySceneFramePipeline.hpp,
    apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp,
    apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp
  - What changed: Added request epoch and options to the snapshot cache key and
    added a same-engine request option change regression test.
  - Why this resolves the finding: The invalidation decision now changes when
    fields consumed by IEphemerisEngine::compute(EphemerisRequest) change, and
    the test proves the snapshot and rendered point recompute without changing
    the engine pointer.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ui-sky-scene-frame-pipeline-tests`: PASS
  - `ctest --test-dir build-ralph -R
    skygate-ui-sky-scene-frame-pipeline-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - apps/skygate-ui/src/scene/SkySceneFramePipeline.hpp
  - apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp
  - apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-041B/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-041B/fix.md

## Remaining concerns
None.

## Final fixer status
READY_FOR_REVIEW
