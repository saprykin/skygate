## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-026
- Title: Implement IAU/IERS frame transformation pipeline
- Source: `IMPLEMENTATION_PLAN.md`; `specs/high-precision-ephemeris-engine.md`
- Base ref: 5f51bf2ff2219a140d8269261bd8425da32890dc
- Head ref: 66ce7843d34a5e9b6fac29e1bddae0074d796e9a

## Summary

HP-026 is an umbrella task over HP-026A through HP-026C. The child task handoffs
and verification reports show the celestial, terrestrial, and orchestration
work mostly landed, and the parent implementation pass recorded that state.
However, two HP-026 child acceptance details remain unmet in the current code:
estimated EOP degradation is not modeled or tested, and skipped-stage
orchestration metadata is not covered.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Estimated EOP degradation is not modeled or tested

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/EarthOrientationProvider.hpp`
Lines/functions: `EarthOrientationDataStatus`, `EarthOrientationSampleWarningCode`

Problem:
HP-026B requires transforms to return degraded metadata when EOP or time data is
missing, stale, predicted, or estimated, and requires tests for those cases.
The current Earth-orientation model exposes `Available`, `Missing`, `Malformed`,
and `Stale` data statuses plus sample warnings for stale, predicted, missing,
out-of-range, and invalid input. There is no estimated EOP state or warning, and
`FrameTransformerTests` only covers predicted, stale, and missing EOP
degradation.

Why it matters:
The parent HP-026 task can only be closed when its child verification items are
complete. As written, callers cannot distinguish estimated EOP data from valid
or other degraded data, and the required estimated degradation behavior cannot
be tested.

Recommended fix:
Add an explicit estimated EOP status or sample warning, propagate it through
`FrameTransformer` metadata as degraded accuracy, and add the corresponding
frame-transform test. If estimated EOP is intentionally out of scope for the
current EOP model, update the task requirement and child handoffs explicitly
before closing HP-026.

### Finding 2: Skipped-stage metadata coverage is missing

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
Lines/functions: `frameRank`, `makeIdentityResult`, `ErfaFrameTransformer::transformCelestialVector`

Problem:
HP-026C verification calls for orchestration tests covering skipped and
unavailable stages. The implementation collapses `Icrs` and `Gcrs` to the same
rank and returns `makeIdentityResult()` whenever source and target ranks match.
That result has no stage metadata, and the existing ICRS/GCRS identity test only
checks the vector and correction flags.

Why it matters:
HP-026C is specifically about preserving per-stage status, warning, provenance,
and applied-correction metadata for result assembly. A skipped identity boundary
is currently silent, so downstream diagnostics cannot tell that a requested
frame boundary was intentionally skipped.

Recommended fix:
Either record an explicit skipped identity stage with `applied=false` and valid
metadata, or clarify that same-rank identity transforms intentionally have no
stage entries. Add focused tests for the chosen behavior, including the
ICRS/GCRS skipped-stage case.

## Test assessment

The source tree contains focused high-precision frame-transform tests for
SOFA/ERFA reference transforms, GCRS/CIRS round trip precision, composed
metadata, time-scale conversion reuse, EOP sample reuse, unavailable-stage
metadata, and predicted/stale/missing EOP degradation. Those tests are gated
behind `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`.

Commands run:

- `git diff --check 5f51bf2..HEAD`: PASS
- `cmake --build build-ralph --parallel`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 58/58 tests
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`: FAIL, `calceph` package config not found
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`: PASS, restored the build tree

The task-specific `skygate-ephemeris-frame-transformer-tests` target could not
be generated or run in this environment because high-precision configuration is
blocked by the missing CALCEPH dependency.

## Regression risk

Medium

The implemented code is isolated to high-precision frame transformation, but it
is numerical infrastructure that later apparent-place and facade paths will
depend on. The configured disabled high-precision suite passes, while the most
relevant gated test binary could not be executed locally.

## Out-of-scope observations

- `IMPLEMENTATION_PLAN.md` still marks HP-026 itself as incomplete. The review
  prompt forbids changing plan files during review, so this report leaves that
  untouched.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
