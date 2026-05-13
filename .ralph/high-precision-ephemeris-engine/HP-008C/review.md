## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-008C
- Title: Preserve simple-engine compatibility overloads
- Source: IMPLEMENTATION_PLAN.md
- Base ref: dea15e0bdaf41800dde66d1a101f057eaa34c042
- Head ref: b89ed5c2e9bffb22cf350e78487a4d5578ba03d7

## Summary

The implementation routes the no-argument and body-span compatibility helpers through the new request/result factory path and adds focused API coverage for no-argument, catalog, span, and request creation. The scoped ephemeris tests pass, and the full suite still has only the pre-existing HP-052 QML failure. However, the new request overload introduces a source-compatibility regression for existing callers that pass `{}` as an empty body span.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Empty-brace factory calls are now ambiguous

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
Lines/functions: lines 214 and 217, `createEphemerisEngine(...)`

Problem:
Adding `createEphemerisEngine(const EphemerisEngineFactoryRequest&)` alongside the existing `createEphemerisEngine(std::span<const CelestialBody>)` makes existing calls of the form `createEphemerisEngine({})` ambiguous. I verified a compile-only snippet that includes `EphemerisEngineFactory.hpp` and calls `skygate::ephemeris::createEphemerisEngine({})`; it fails at HEAD with overload ambiguity between the request overload and the span overload.

Why it matters:
HP-008C is specifically a public API compatibility task. Existing simple-engine callers could reasonably use `{}` to request an empty body span, and this change breaks those callers at compile time.

Recommended fix:
Adjust the overload set so empty-brace calls continue to resolve to the simple-engine compatibility path, or add an explicit compatibility overload for the empty initializer case. Add API coverage that compiles and calls `createEphemerisEngine({})` to prevent this regression from returning.

## Test assessment

The new `skygate-ephemeris-api-model-tests` coverage exercises the no-argument, catalog, body-span, and request-result factory paths, and the existing baseline, fallback, and regression tests cover golden simple-engine behavior. Missing coverage is the source-compatibility compile case for `createEphemerisEngine({})`.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests -j2`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-baseline|engine-fallback|regression)-tests'`: PASS, 4/4
- `ctest --test-dir build-ralph --output-on-failure`: 103/104 PASS, with only the pre-existing `skygate-ui-qml-main-window-tests` HP-052 failure at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp:219`
- Temporary compile-only check for `createEphemerisEngine({})`: FAIL with overload ambiguity

## Regression risk

Medium

The implementation is small and task-scoped, but the affected surface is a public factory API and the ambiguity is a compile-time source-compatibility break.

## Out-of-scope observations

- The full suite still fails `skygate-ui-qml-main-window-tests` in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`, which is already tracked by HP-052 and is unrelated to HP-008C.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
