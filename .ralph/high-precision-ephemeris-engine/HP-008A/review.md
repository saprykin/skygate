## Verdict

PASS

## Task reviewed

- ID: HP-008A
- Title: Define factory request and fallback policy model
- Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md
- Base ref: a97047df74f499932c74f47e5eaedba662e3eb9d
- Head ref: 3e0d5e708c5c0a5ae134e19e78fe4a880eb31687

## Summary

The implementation adds the API-only factory request model, opaque forward-declared provider/data/diagnostics handles, and a fallback policy enum that distinguishes strict high precision from explicit simple-engine fallback. The added API model tests cover request defaults, simple and high-precision request construction, and fallback policy helpers. The task-scoped implementation satisfies HP-008A.

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

The new coverage in `skygate-ephemeris-api-model-tests` constructs default, simple, and high-precision `EphemerisEngineFactoryRequest` values and verifies fallback policy defaults plus strict/fallback helper behavior. This is appropriate for the API-only scope of HP-008A. The focused build target passed, and the focused CTest run for `skygate-ephemeris-(api-model|engine-interface-migration)-tests` passed.

The full `ctest --test-dir build-ralph --output-on-failure` run passed 103/104 tests and failed only the pre-existing `skygate-ui-qml-main-window-tests` case `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Regression risk

Low

The change is limited to public API model declarations in the ephemeris factory header and compile/API-style tests. Existing simple-engine factory overload declarations remain available.

## Out-of-scope observations

- The full suite still has the unrelated QML footer popup toolbar regression already captured by HP-052.

## Final recommendation

PASS: ready for final verification.
