## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-026C
- Title: Add `FrameTransformer` orchestration and per-stage metadata
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: 4ff76a5a8ae762033fd8ca0eacae35f496f9dee1
- Head ref: 10ffea63b735e027c4c5f4c062be8684cb9b794b

## Summary

The implementation adds stage metadata to `CelestialFrameTransformResult`, introduces a request-scoped transform context, and adds tests for composed metadata, conversion caching, and unavailable stages. The general shape matches HP-026C, but the composed terrestrial path still duplicates Earth-orientation sampling through the UT1 conversion path and the polar-motion path, so one of the task's core acceptance criteria is not met.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Composed terrestrial transforms still duplicate EOP lookup work

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
Lines/functions: `FrameTransformContext::epochInScale`, `FrameTransformContext::earthOrientation`, `intermediateToTerrestrialIntermediateMatrix`

Problem:
The request context caches direct `FrameTransformer` Earth-orientation sampling, but a composed transform that reaches ITRS still samples EOP through two independent paths. The CIRS/TIRS stage calls `epochInScale(TimeScale::Ut1)`, which delegates to `ITimeScaleService::convert`. The production `LeapSecondTimeScaleService` uses EOP internally for UTC-to-UT1 conversion. The later TIRS/ITRS stage then calls `FrameTransformContext::earthOrientation()`, which samples EOP again for polar motion.

Why it matters:
HP-026C explicitly requires composed stages to avoid duplicating time-scale or EOP lookup work per request. This path can also make UT1 and polar-motion metadata inconsistent if the time-scale service and frame transformer are wired with different EOP providers or fallback options.

Recommended fix:
Make the composed frame transform obtain the needed EOP sample once per request and reuse it for both UT1 and polar-motion data, or expose/share a single sampled EOP result between the time-scale conversion and frame-transform stages. Add a test with a recording EOP provider that proves a GCRS/CIRS-to-ITRS request samples EOP only once.

### Finding 2: ICRS requests are reported as GCRS stages

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
Lines/functions: `frameForRank`, `ErfaFrameTransformer::transformCelestialVector`

Problem:
`Icrs` and `Gcrs` intentionally share rank 0, but stage metadata is reconstructed from rank with `frameForRank(0)`, which always returns `Gcrs`. For an `Icrs -> Cirs` or `Icrs -> Itrs` request, the first recorded stage is `Gcrs -> Cirs`; for a reverse transform targeting `Icrs`, the final recorded stage is `Cirs -> Gcrs`.

Why it matters:
The vector math may be acceptable because ICRS and GCRS are treated as identity axes here, but HP-026C is specifically about preserving per-stage metadata for result assembly. Reporting a different requested frame path makes downstream diagnostics and applied-stage provenance misleading.

Recommended fix:
Preserve the request's actual boundary frame in the first or last stage metadata, or record an explicit identity `Icrs <-> Gcrs` stage with `applied=false`/no corrections before composing the physical stages. Add tests for `Icrs -> Itrs` and `Itrs -> Icrs` stage metadata.

## Test assessment

New `FrameTransformerTests` coverage was added for composed-stage metadata, cached conversion reuse, and one unavailable-stage case. The tests do not currently prove EOP sampling is reused across UT1 and polar-motion stages, and they do not cover ICRS boundary metadata.

I ran `ctest --test-dir build-ralph --output-on-failure` with the existing `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` build; all 58 configured tests passed. I also attempted to configure `build-ralph` with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`, but configuration failed because `calceph` is not installed, so the new `skygate-ephemeris-frame-transformer-tests` target could not be built or run in this environment.

## Regression risk

Medium

The touched code is localized to high-precision frame transforms, but the issue is in the orchestration path that will feed apparent-place and facade result assembly.

## Out-of-scope observations

The new tests only partially assert per-stage correction metadata for composed transforms; stages 1 and 2 are checked for frame order but not for `EarthOrientation` correction flags. This should be covered when fixing the metadata tests above.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
