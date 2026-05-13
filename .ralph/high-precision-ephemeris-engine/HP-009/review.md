## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-009
- Title: Add the high-precision engine facade boundary
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 0733e9f16fc86202c474fd1cd03879226502b567
- Head ref: bf25e9be12b4861662932fa1d7387c61ea66e4d4

## Summary

The implementation adds `HighPrecisionEphemerisEngine`, internal collaborator interfaces, facade dispatch, request validation, structured unsupported/failed states, CMake wiring, and a new Qt test target. The core facade behavior is scoped to HP-009 and the task-specific test passes, but one claimed acceptance criterion is not actually covered: the option-forwarding test would pass even if the facade forwarded constructor options instead of per-request options.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Option-forwarding test does not distinguish request options from engine defaults

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
Lines/functions: `forwardsOptionsThroughCollaboratorsAndResultBuilder`, lines 390-394

Problem:
The test mutates `request.options.correctionFlags` and then constructs `HighPrecisionEphemerisEngine` with that same `request.options`. Because the constructor options and per-call request options are identical, the test would still pass if the facade incorrectly forwarded `m_options` instead of `input.request.options` to collaborators.

Why it matters:
HP-009 explicitly claims option forwarding is covered by tests. Per-request options are part of the new facade boundary, and future callers need to be able to vary correction flags per request. The current test does not protect that contract.

Recommended fix:
Construct the engine with intentionally different default options from the `EphemerisRequest` options, then assert that the calculators, apparent-place calculator, result builder, and returned metadata observe the request-specific flags.

### Finding 2: High-precision facade tests are not discoverable through the highprecision label

Severity: MINOR
File: `libs/skygate-ephemeris/tests/CMakeLists.txt`
Lines/functions: `add_skygate_ephemeris_qtest(skygate-ephemeris-highprecision-engine-tests ...)`, lines 136-139

Problem:
The new facade test target is added without `LABELS "unit;highprecision"`, so it defaults to only the `unit` label. In the current `build-ralph` tree, `ctest -L highprecision -N` finds no tests even though HP-009 added the main high-precision facade coverage.

Why it matters:
The existing high-precision smoke tests use the `highprecision` label when enabled. Without the label, focused high-precision verification can skip the HP-009 tests unintentionally.

Recommended fix:
Add `LABELS "unit;highprecision"` to the `skygate-ephemeris-highprecision-engine-tests` registration.

## Test assessment

Added tests cover facade metadata/capabilities, solar-system and star dispatch, failed request validation, unsupported body handling, and basic result assembly with fake collaborators. `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests` passed, and `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision-engine-tests --output-on-failure` passed.

The full suite was also run with `ctest --test-dir build-ralph --output-on-failure`; it reported 105/107 passing. The two failures were `skygate-ui-qml-main-window-tests` and `skygate-ui-qml-main-window-rendering-tests`, matching unrelated UI failures noted in the implementation handoff.

## Regression risk

Low

The production code added for HP-009 is internal to the ephemeris library and is not factory-wired yet. The main risk is insufficient test protection around per-request option forwarding before later tasks wire this facade into real high-precision construction.

## Out-of-scope observations

- `specs/high-precision-ephemeris-engine.md` from the review prompt does not exist; the repository uses `spec/high-precision-ephemeris-engine.md`.
- The facade defines `highprecision::ITimeScaleService` and `highprecision::IEarthOrientationProvider`, while the factory request already forward-declares similarly named `skygate::ephemeris` interfaces. HP-008F may need either shared interface types or explicit adapters when real factory wiring is implemented.
- The full CTest run still has the two unrelated QML failures recorded by the implementer.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
