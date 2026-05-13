# Verdict

PASS

# Task verified

- ID: HP-008F
- Title: Wire real high-precision construction once dependencies exist
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: `76f2bd219d20bdce31742970420745fefda19e7b`
- Head ref: `4a3558c75d7a6200c08424768ce0707a2c7fea3f`

# Summary

The factory now constructs a high-precision engine when supplied with an active data snapshot, manifest, time-scale service, Earth-orientation provider, and ready CALCEPH runtime. The review found two major gaps: diagnostics sink delivery and apparent/topocentric provider wiring. The fix pass addressed both by publishing factory diagnostics to the request sink and wiring `ApparentPlaceCalculator` into factory-created high-precision engines. The focused tests and full configured `build-ralph` suite pass, and the task is ready for acceptance.

# Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read review report
- [x] Read fixer report, if present
- [x] Read relevant specs
- [x] Inspected git history
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

# Review/fix closure

- Finding: Diagnostics sink is accepted but never used
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `IEphemerisDiagnosticsSink::recordFactoryCreationDiagnostic(...)` is now defined and `createEphemerisEngine(const EphemerisEngineFactoryRequest&)` publishes returned diagnostics to the request sink. Factory behavior tests cover fallback warnings and strict errors reaching a recording sink.

- Finding: Apparent/topocentric correction path is not wired for factory-created engines
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Factory high-precision construction now creates an `ApparentPlaceCalculator` with the frame transformer, time-scale service, and Earth-orientation provider. Factory behavior tests request apparent/topocentric output and verify the propagated time/EOP providers are consumed.

# Findings

No findings.

# Test assessment

Relevant factory and high-precision tests exist for successful fake-runtime high-precision construction, data-set and kernel metadata propagation, provider consumption on the apparent/topocentric path, strict high-precision failure, explicit simple fallback, and diagnostics sink publishing.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris skygate-ephemeris-engine-factory-behavior-tests skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-api-model-tests` - PASS
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(engine-factory-behavior|engine-factory-selection|highprecision-engine|api-model)-tests'` - PASS, 4/4
- `ctest --test-dir build-ralph --output-on-failure -L highprecision` - PASS, 9/9
- `ctest --test-dir build-ralph --output-on-failure` - PASS, 58/58

The current `build-ralph` configuration has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so dependency-enabled CALCEPH configuration was not rerun in this verifier pass. The fake runtime coverage exercises the factory wiring required by this task.

# Regression risk

Low

The changes are focused on factory construction, diagnostics publishing, and high-precision apparent-place dependency wiring. Existing simple-engine compatibility and the full configured test suite pass.

# Out-of-scope observations

- A separate high-precision-enabled build with real CALCEPH remains useful once `calceph_DIR` is available in the verification environment.

# Final recommendation

PASS: ready for final acceptance or merge.
