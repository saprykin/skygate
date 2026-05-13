## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-006B
- Title: Add request-based snapshot compute API
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 2962442dedf2cbbdbff65a0282ba1e63c612196a
- Head ref: a8395dbaeca1f725c14a604466d8654e2425a085

## Summary

The implementation adds `IEphemerisEngine::compute(const EphemerisRequest&)`, overrides it in the simple engine, and adds a baseline test proving a UTC `AstronomicalEpoch` can replace `request.context.utcTime` while preserving the existing `SkySnapshot` shape. The main gap is that the simple-engine request adapter does not use or validate the request option fields, even though HP-006B explicitly requires the request path to use epoch, observer, and option fields.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Request options are ignored by the simple-engine request path

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: `contextFromRequest`, `SimpleEphemerisEngine::compute(const EphemerisRequest&)`

Problem:
`compute(const EphemerisRequest&)` only converts `request.epoch` into `context.utcTime` for UTC epochs and then delegates to the existing `SkyContext` overload. `request.options` is never read, validated, normalized to the simple engine's supported options, or reflected in result metadata. The new test assigns `request.options = engine->options()`, so it also would not catch this omission.

Why it matters:
HP-006B requires the simple-engine request path to use the request's epoch, observer, and option fields. With the current implementation, callers can pass different correction/refraction options and receive identical results with no indication that the options were ignored or unsupported. That makes the new public request API misleading at the boundary where later high-precision behavior is meant to plug in.

Recommended fix:
Update the request adapter to consume the supported option surface explicitly. For unsupported simple-engine corrections or atmospheric refraction requests, either normalize to simple-engine options and mark per-result metadata with `CorrectionUnavailable`/degraded status where appropriate, or define and test the intended fallback behavior. Add tests that pass non-default request options and verify the simple-engine response matches that contract.

## Test assessment

`skygate-ephemeris-engine-baseline-tests` now includes request-based snapshot coverage, and it passed. The test proves a UTC request epoch overrides `request.context.utcTime` and that observer fields in `request.context` still drive horizontal coordinates. It does not cover request option behavior, unsupported option handling, or the default-constructed request option path.

I also ran the full `ctest --test-dir build-ralph --output-on-failure` suite. It passed 102/103 tests; the only failure was the pre-existing HP-051 `skygate-ui-qml-main-window-tests` footer popup toolbar regression.

## Regression risk

Medium

The added overload is source-compatible and the old `SkyContext` path still passes existing tests. The risk is semantic: the new request API accepts option fields but silently ignores them in the simple-engine adapter.

## Out-of-scope observations

- The review prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout contains `spec/high-precision-ephemeris-engine.md`.
- Existing fake `IEphemerisEngine` implementations inherit the default request overload, which delegates to `compute(request.context)`. That is acceptable for preserving compatibility in this child task, but HP-006E should update fakes/tests so future request-path tests do not accidentally ignore request-only fields.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
