## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-049
- Title: Restore missing public high-precision API model types
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 3cfb669
- Head ref: 381adc50e6deec94520a40837fc638c4f9159175

## Summary

The implementation adds the missing HP-004 public API model types to
`Types.hpp` and registers focused compile/API coverage for construction,
defaults, correction flag composition, request/data-set models, and the two
engine kinds. The configured `build-ralph` suite passes. However, the new public
correction flag enumerator `None` creates a macro-collision hazard for existing
simple-engine clients because `Types.hpp` is transitively included by the
current engine interface.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Correction flag `None` can break public header consumers

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
Lines/functions: line 36, `EphemerisCorrectionFlags`

Problem:
`EphemerisCorrectionFlags` introduces an enumerator named `None`. That token is
also defined as a macro by X11 headers. Because `Types.hpp` is included by
`IEphemerisEngine.hpp`, any existing Linux/X11 client that includes an X11
header before the Skygate ephemeris engine interface can fail during public
header parsing before it uses any high-precision API.

Why it matters:
HP-049 is a public API/modeling follow-up that should preserve existing
simple-engine clients where practical. Adding a macro-sensitive public
enumerator to a transitive core header creates an avoidable source compatibility
regression.

Recommended fix:
Rename the zero-value correction flag to a less collision-prone API name such
as `NoCorrections` or `NoCorrectionFlags`, and update `hasCorrectionFlag`,
defaults, and the API model tests accordingly.

## Test assessment

The new `skygate-ephemeris-api-model-tests` target covers the restored public
types, default construction, flag combination, request/data-set construction,
and exactly two engine kinds. I built `skygate-ephemeris` and
`skygate-ephemeris-api-model-tests` in `build-ralph`, ran
`clang-format --dry-run --Werror` on the touched C++ files, and ran
`ctest --test-dir build-ralph --output-on-failure`; all 45 configured tests
passed.

## Regression risk

Medium

The implementation is small and compile/API focused, but it changes a widely
included public header used by existing simple-engine consumers.

## Out-of-scope observations

`EphemerisWarning` and `EphemerisResultStatus` remain absent from `Types.hpp`;
the implementation plan tracks those under HP-005 rather than this HP-004
follow-up.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
