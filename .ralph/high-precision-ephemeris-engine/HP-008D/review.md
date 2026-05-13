## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-008D
- Title: Implement factory selection for simple and unavailable high precision
- Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md
- Base ref: b276d1ec3ab040cee4a83b9f4f6ba0b5493fce61
- Head ref: 820eca6894f836229b0b5a3917870139a357c932

## Summary

The implementation adds factory selection for simple requests, high-precision unavailable fallback, strict high-precision failure, and focused factory selection tests. The simple and strict paths are generally implemented, and the relevant tests pass. However, high-precision fallback is not actually limited to explicit opt-in because the factory request defaults to allowing fallback, so a caller can request high precision and still silently receive the simple engine without setting a fallback policy.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: High-precision fallback is allowed by default

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`; `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: `EphemerisEngineFactoryRequest::fallbackPolicy` line 52; `createEphemerisEngine(const EphemerisEngineFactoryRequest&)` lines 354-360

Problem:
`EphemerisEngineFactoryRequest::fallbackPolicy` defaults to `AllowSimpleEngineFallback`, and the new factory selection code falls back whenever `allowsSimpleEngineFallback(request.fallbackPolicy)` is true. As a result, a caller that only sets `request.engineKind = EphemerisEngineKind::HighPrecision` receives a simple-engine fallback even though it did not explicitly opt into fallback. The added tests cover the explicit allow and explicit strict cases, but they do not cover a high-precision request with the default fallback policy.

Why it matters:
HP-008D requires high-precision requests to return the simple engine only when fallback is explicitly allowed. The spec also frames fallback as caller-requested behavior. Returning a fallback engine by default makes strict high-precision selection too easy to miss and can mask unavailable high-precision data/dependencies behind a simple engine result.

Recommended fix:
Make high-precision fallback opt-in for executable factory behavior. The most direct fix is to default `EphemerisEngineFactoryRequest::fallbackPolicy` to `StrictHighPrecision`, update the API model default test accordingly, and add a factory selection test proving that `HighPrecision` with the default policy returns `FailedStrictHighPrecisionUnavailable` and no engine. Keep the compatibility overloads explicitly creating simple-engine requests so existing simple-engine callers remain unaffected.

## Test assessment

The new `skygate-ephemeris-engine-factory-selection-tests` target covers simple creation, explicit high-precision fallback, strict high-precision failure, catalog body propagation, selected option propagation, and non-empty diagnostics. The focused target builds and passes, and the related API/interface/factory CTest selection passes.

The missing coverage is the default-policy high-precision request path described in Finding 1. Without that test, the implementation can violate the "explicitly allowed" fallback acceptance criterion while all new tests still pass.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris-engine-factory-selection-tests`
- `ctest --test-dir build-ralph -R skygate-ephemeris-engine-factory-selection-tests --output-on-failure`
- `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|engine-interface-migration|engine-factory-selection)-tests" --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

Full CTest result was 104/105 passing. The only failure was the pre-existing unrelated `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, already tracked by HP-052.

## Regression risk

Medium

The implementation touches central factory behavior and changes what a high-precision request returns when high precision is unavailable. The changed source surface is small, but the default fallback behavior can affect future callers that expect high precision to fail unless they opted into fallback.

## Out-of-scope observations

- The REVIEW prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout contains the spec at `spec/high-precision-ephemeris-engine.md`.
- The unrelated QML main-window test failure remains present and is already tracked by HP-052.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
