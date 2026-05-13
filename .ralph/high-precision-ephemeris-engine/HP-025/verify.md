# Verdict

PASS

# Task verified

- ID: HP-025
- Title: Implement geometric solar-system vector calculation
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 0220ef6485c2bd9b4c37f8178a89987aa21dd024
- Head ref: 3d21348aad1da556266fd72b2d751a2dab788ae1

# Summary

HP-025 adds a high-precision `SolarSystemStateCalculator`, CALCEPH-backed geocentric geometric vectors, NAIF mappings for supported major solar-system bodies, facade dispatch for solar-system states, and deterministic coverage including a compact Horizons geometric fixture. The review identified three issues around geometric requests, result metadata lifetime, and non-TDB provider inputs. The fix pass addressed all three with code changes and regression tests. Targeted ephemeris tests pass. The full suite still has the pre-existing unrelated QML main-window footer popup failure tracked separately as HP-056, so HP-025 is ready for acceptance.

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

- Finding: Geometric requests can still be modified by apparent-place processing
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `HighPrecisionEphemerisEngine::computeStateForBody` now bypasses apparent-place processing when the request correction flags are `NoCorrections`/`Geometric`, and `bypassesApparentPlaceForGeometricSolarSystemRequests()` verifies a mutating apparent-place calculator is not called.

- Finding: Result metadata can dangle after provider lifetime ends
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `EphemerisResultMetadata` now owns provenance and effective validity range values. `CalcephKernelProvider::computeGeometricState` copies kernel metadata into the result, and provider coverage reads the metadata after provider destruction.

- Finding: Kernel provider accepts non-TDB epochs directly
  - Original severity: MINOR
  - Closure status: Resolved
  - Notes: `CalcephKernelProvider::computeGeometricState` rejects non-TDB epochs with `Failed` and `TimeScaleDataUnavailable` before calling the kernel handle. The new provider test verifies the kernel is not invoked.

# Findings

No findings.

# Test assessment

Relevant tests exist for the solar-system calculator, CALCEPH provider boundary, high-precision facade dispatch, API metadata model, fallback behavior, simple-engine baseline, and regression suite. They cover NAIF ID mapping, vector-to-RA/Dec conversion, unsupported bodies, missing providers, out-of-range kernel status, non-TDB rejection at both calculator and provider boundaries, owned metadata, and the geometric/no-apparent facade path.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-calceph-kernel-provider-tests skygate-ephemeris-solar-system-state-calculator-tests skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|api-model|engine-fallback|engine-baseline|regression)-tests'`: PASS, 7/7 tests passed
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, 115/116 tests passed; the only failure was the unrelated known `skygate-ui-qml-main-window-tests` footer popup toolbar assertion at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, tracked separately as HP-056

Optional DE441 long-range fixture coverage is still absent because no installed long-range kernel is available in this test setup; that is acceptable for this task because the plan made it optional when installed.

# Regression risk

Low

The new production code is isolated under the high-precision engine boundary and covered by focused tests. The fix pass touched shared result metadata ownership, but the API model and fallback tests were updated and passed. Existing simple-engine baseline and regression tests also passed.

# Out-of-scope observations

- The full-suite QML footer popup toolbar toggle failure remains outside HP-025 and is already tracked as HP-056.
- Public factory wiring for constructing a complete high-precision engine with real data remains future work outside this task.

# Final recommendation

PASS: ready for final acceptance or merge.
