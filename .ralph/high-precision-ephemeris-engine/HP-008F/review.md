## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-008F
- Title: Wire real high-precision construction once dependencies exist
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: `76f2bd219d20bdce31742970420745fefda19e7b`
- Head ref: `8f01f36b0f0e5f22bd6ebd291b4db907fa4b559d`

## Summary

The factory now constructs a `HighPrecisionEphemerisEngine` when supplied with a data snapshot, manifest, time-scale service, Earth-orientation provider, and a ready CALCEPH runtime. Strict failure and explicit simple fallback paths are present and covered by tests.

However, two pieces of the HP-008F wiring are incomplete: the request diagnostics sink is ignored, and constructed engines are not wired with an apparent/topocentric correction calculator even though the factory accepts correction options and installs time/EOP/frame dependencies. The task needs another fix pass.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Diagnostics sink is accepted but never used

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: `createHighPrecisionEphemerisEngine`, lines 407-479

Problem:
HP-008F requires construction using the request diagnostics sink, and the public request exposes `IEphemerisDiagnosticsSink* diagnosticsSink`. The real high-precision construction path never reads `request.diagnosticsSink`, never forwards it to the created engine or dependencies, and never emits creation diagnostics through it on strict or fallback failures.

Why it matters:
Callers can provide a diagnostics sink and get no effect, so factory diagnostics cannot be observed through the API field intended for that purpose. This also leaves the task's diagnostics-sink requirement unimplemented.

Recommended fix:
Define the concrete diagnostics sink contract if it is still opaque, then emit high-precision construction diagnostics to `request.diagnosticsSink` or propagate the sink into the high-precision dependencies where runtime diagnostics are produced. Add a test with a real recording sink that fails before the fix.

### Finding 2: Apparent/topocentric correction path is not wired for factory-created engines

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: `createHighPrecisionEphemerisEngine`, lines 450-466

Problem:
The factory wires `timeScaleService`, `earthOrientationProvider`, and an `ErfaFrameTransformer`, but leaves `HighPrecisionEphemerisEngineDependencies::apparentPlaceCalculator` unset. `HighPrecisionEphemerisEngine` therefore falls back to `DefaultApparentPlaceCalculator`, which returns calculator results unchanged. A request using apparent/topocentric correction flags can still create a high-precision engine whose normal compute path does not consume the propagated time/EOP/frame dependencies for those corrections.

Why it matters:
The factory can advertise high-precision capabilities and preserve requested correction flags while returning geometric solar-system results for apparent/topocentric requests. That is fragile API behavior and does not satisfy the task's provider-propagation intent for real construction wiring.

Recommended fix:
Wire an actual apparent/topocentric calculator into `HighPrecisionEphemerisEngineDependencies` when the factory constructs the real high-precision engine, or explicitly downgrade capabilities/options and diagnostics until that calculator exists. Add a factory test that requests apparent/topocentric output and proves the time-scale service and Earth-orientation provider are used.

## Test assessment

Factory tests cover the successful fake-runtime construction path, manifest/snapshot kernel metadata propagation, strict high-precision failure, and explicit simple fallback. The full existing `build-ralph` CTest suite passes in the current high-precision-disabled build.

Coverage is still weak for HP-008F's provider and diagnostics wiring. No test uses a real diagnostics sink, and the successful construction test uses a geometric Sun request, so it does not prove that time-scale or Earth-orientation providers are consumed by a factory-created high-precision engine.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris-engine-factory-behavior-tests skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-api-model-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(engine-factory-behavior|engine-factory-selection|highprecision-engine|api-model)-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

## Regression risk

Medium

The core simple fallback behavior still passes existing tests, but the new high-precision factory path exposes public options and provider inputs that are not fully honored.

## Out-of-scope observations

- `build-ralph` is currently configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`; the fake CALCEPH runtime gives useful coverage, but this review did not reconfigure an ON build with real external dependencies.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
