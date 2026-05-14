## Verdict

PASS

## Task reviewed

- ID: HP-042
- Title: Extend scene and app cache keys for high precision
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: fbce8ae80f5cd301b017b0019e2a32e861524f0b
- Head ref: da91576337778e67a721784453fe5b200364a4e7

## Summary

The implementation extends the scene snapshot cache key with engine kind,
engine options revision, ephemeris data revision, Earth-orientation revision,
leap-second revision, request epoch, and request options. `SkySceneModel` now
passes the controller's high-precision request context into the pipeline, and
the controller derives data revisions from the active ephemeris data snapshot.

The changes satisfy HP-042. The focused cache-invalidation tests pass, and the
full configured `build-ralph` suite passes with only the existing
CALCEPH-dependent tests skipped.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

No findings.

## Test assessment

The implementation adds focused `SkySceneFramePipeline` coverage for engine
kind, options revision, ephemeris data revision, EOP revision, leap-second
revision, subsecond astronomical epoch changes, and render-only changes.
Existing controller tests cover construction of request contexts from restored
engine settings, observer, time, and data revision.

I ran:

```sh
cmake --build build-ralph \
  --target skygate-ui-sky-scene-frame-pipeline-tests -j2
ctest --test-dir build-ralph \
  -R skygate-ui-sky-scene-frame-pipeline-tests --output-on-failure
ctest --test-dir build-ralph \
  -R skygate-ui-context-controller-ephemeris-settings-tests \
  --output-on-failure
ctest --test-dir build-ralph \
  -R skygate-ui-context-controller-selected-engine-matrix-tests \
  --output-on-failure
ctest --test-dir build-ralph --output-on-failure
```

All runnable tests passed. The full suite reported 123 passed tests and skipped
the two existing CALCEPH-dependent tests.

## Regression risk

Low

The production changes are localized to cache-key construction and request
context propagation. Existing render-frame caching remains separate, and
render-only changes still avoid ephemeris recomputation.

## Out-of-scope observations

None.

## Final recommendation

PASS: ready for final verification.
