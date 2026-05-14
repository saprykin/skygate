## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-029
- Title: Implement atmospheric refraction calculation
- Source: `IMPLEMENTATION_PLAN.md` HP-029
- Base ref: 009f24cc1628c23422aec70fbc8523ee970f1c95
- Head ref: c3b79e732132a370ff41862b66a155f6ff8c2938

## Summary

The implementation adds `AtmosphericRefractionCalculator`, wires it into the
high-precision apparent-place pipeline, and applies Bennett-style altitude
refraction only when the request enables the atmospheric-refraction flag.
Focused behavior and integration tests pass.

The main implementation path appears sound, but the task's verification
requirements and handoff claims are not fully covered by tests. In particular,
invalid observer input and most atmosphere input validation branches are
untested.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Refraction input validation coverage is incomplete

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`
Lines/functions: `reportsInvalidAtmosphereInputs()`,
`AtmosphericRefractionCalculatorTests`

Problem:
HP-029 explicitly requires tests for missing atmosphere inputs, boundary
altitude behavior, and warning metadata. The handoff also claims invalid
observer, invalid atmosphere, and out-of-range altitude metadata coverage.
Current tests cover missing horizontal coordinates, one invalid pressure case,
one low-altitude case, disabled mode, and near-zenith correction. They do not
cover invalid observer input, nor the temperature, relative-humidity,
wavelength, or non-finite atmosphere input branches validated by
`hasValidAtmosphere()`.

Why it matters:
The calculator's availability contract depends on rejecting bad observer and
atmosphere inputs without applying refraction, while reporting degraded
`CorrectionUnavailable` metadata. Since most of that validation surface is not
exercised, future changes could silently apply refraction with invalid inputs
or stop reporting the required warning metadata.

Recommended fix:
Add focused tests that request atmospheric refraction with an invalid observer
and with invalid or non-finite temperature, relative humidity, and wavelength
values. Each case should assert that altitude remains unchanged,
`AtmosphericRefraction` is not marked as applied, status is degraded when the
input result was valid, and `CorrectionUnavailable` is present. Keep the
existing pressure and altitude boundary tests.

## Test assessment

Added tests cover enabled refraction, disabled behavior, missing horizontal
coordinates, one invalid pressure case, low-altitude rejection, near-zenith
behavior, and integration through `ApparentPlaceCalculator`.

Commands run:

- `cmake --build build-ralph --target
  skygate-ephemeris-atmospheric-refraction-calculator-tests
  skygate-ephemeris-apparent-place-calculator-tests`
- focused CTest filter for atmospheric-refraction, apparent-place,
  highprecision-engine, and engine-factory-behavior tests
- `ctest --test-dir build-ralph --output-on-failure`

Focused ephemeris tests passed: 4/4. The full suite passed 120/121 tests, with
only `skygate-ui-qml-main-window-rendering-tests` failing on the previously
noted out-of-scope `SkyOverlayLabel` bounds issue.

## Regression risk

Low

The functional changes are scoped to the high-precision apparent-place path and
the simple-engine fallback behavior remains unchanged. The remaining concern is
test coverage for the new input-validation contract.

## Out-of-scope observations

- `skygate-ui-qml-main-window-rendering-tests` still fails because a
  `SkyOverlayLabel` is above the viewport. This matches the unrelated failure
  described in the HP-029 implementation handoff.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
