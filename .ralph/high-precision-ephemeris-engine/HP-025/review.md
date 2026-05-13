## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-025
- Title: Implement geometric solar-system vector calculation
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 0220ef6485c2bd9b4c37f8178a89987aa21dd024
- Head ref: 4edd30626c94d63a76fc201f7717be66116d1d9a

## Summary

The pass adds a CALCEPH-backed kernel state API, a `SolarSystemStateCalculator`,
and focused tests for NAIF mapping, vector-to-RA/Dec conversion, and structured
failure cases. The core vector-to-equatorial math looks correct, and the
targeted ephemeris tests pass. However, the high-precision facade can still
apply apparent-place processing to a geometric request, and the provider returns
metadata views/pointers into provider-owned storage that can dangle after a
snapshot escapes the engine/provider lifetime.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Geometric requests can still be modified by apparent-place processing

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
Lines/functions: `HighPrecisionEphemerisEngine::computeStateForBody`, lines 287-292

Problem:
After the solar-system calculator returns geometric RA/Dec, the facade always
passes the result through `apparentPlaceCalculator(...)`. If a non-identity
apparent calculator dependency is installed, a request whose correction flags
are `Geometric`/`NoCorrections` can still have its RA/Dec changed.

Why it matters:
HP-025 explicitly says not to implement apparent corrections in this task and
requires geometric request outputs. The current facade path does not preserve
that boundary once an apparent-place collaborator is present.

Recommended fix:
Gate the apparent-place stage on requested correction flags, and bypass it for
geometric/no-correction requests. Add a facade test that wires a mutating
apparent calculator, sends a geometric solar-system request, and verifies the
geometric RA/Dec and `appliedCorrections` are preserved.

### Finding 2: Result metadata can dangle after provider lifetime ends

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
Lines/functions: `CalcephKernelProvider::computeGeometricState`, lines 333-335

Problem:
`computeGeometricState()` stores `dataSourceProvenance` as a `string_view` into
`m_kernelInfo->provenance` and `effectiveDataValidityRange` as a pointer to
`m_kernelInfo->validityRange`. That metadata is copied into calculator results
and then into public `CelestialBodyState` values.

Why it matters:
`SkySnapshot` and `CelestialBodyState` are returned by value and can outlive the
engine/provider. In that case, the public metadata contains dangling references,
which can become use-after-free when UI or diagnostics code reads provenance or
validity-range data.

Recommended fix:
Return metadata that references storage with a lifetime at least as long as the
public result, or change the metadata model used here to own/copy these fields.
At minimum, do not point state metadata directly into `CalcephKernelProvider`
members unless result lifetime is explicitly tied to the provider and tested.

### Finding 3: Kernel provider accepts non-TDB epochs directly

Severity: MINOR
File: `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
Lines/functions: `statusForEpoch`, `computeGeometricState`, lines 301-338

Problem:
`SolarSystemStateCalculator` rejects non-TDB epochs, but
`CalcephKernelProvider::computeGeometricState()` only checks finiteness and
date range before passing the epoch parts to CALCEPH. Direct provider callers
can pass UTC, TT, or UT1 epochs and receive plausible but wrong vectors.

Why it matters:
The provider is now the kernel boundary for geometric solar-system states, and
HP-025 claims non-TDB epoch cases return structured status/warnings. Enforcing
that only in one caller leaves the provider contract fragile.

Recommended fix:
Reject non-TDB epochs in `CalcephKernelProvider::computeGeometricState()` with
`Failed` plus `TimeScaleDataUnavailable`, or document that the provider accepts
only normalized TDB and add an assertion/test around that contract.

## Test assessment

The new `skygate-ephemeris-solar-system-state-calculator-tests` cover the
standalone calculator's RA/Dec conversion, NAIF mapping, unsupported bodies,
missing provider, out-of-range propagation, and non-TDB rejection. They do not
cover the real high-precision facade using `SolarSystemStateCalculator`, so the
geometric/no-apparent boundary issue is not caught. The fixture smoke test is
useful, but the RA tolerance is stored as degrees and compared against RA hours,
which should be tightened or converted for clarity.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-solar-system-state-calculator-tests`
- `cmake --build build-ralph --target skygate-ephemeris-calceph-kernel-provider-tests`
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|engine-baseline|engine-fallback|regression)-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

Targeted ephemeris tests passed. Full CTest ran 116 tests with 115 passing; the
only failure was the pre-existing `skygate-ui-qml-main-window-tests`
`footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` failure at
`apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Regression risk

Medium

The new calculator is isolated, but it plugs into shared result metadata and
the facade correction pipeline. Those areas affect every high-precision
solar-system result once real construction is wired.

## Out-of-scope observations

- The public factory still reports high precision as unavailable; that appears
  to predate this task and should be handled under the factory-wiring work, not
  as an HP-025 blocker.
- Optional DE441 long-range geometric coverage is not present in the new test
  target; add it when an installed long-range kernel is available in CI.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
