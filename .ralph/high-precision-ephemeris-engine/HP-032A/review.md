## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-032A
- Title: Add immutable cache-friendly catalog astrometry arrays
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 5f2cf3ebde9d79cce38e3705b562f157a09d5964
- Head ref: 0c0778edfe62c037c433347108d3fd90f7bd2af7

## Summary

The implementation adds an immutable snapshot type for catalog-star astrometry
data, wires it into the ephemeris build, and covers full astrometry, partial
astrometry, fixed-only stars, and source lifetime independence with tests.

The core snapshot behavior is covered and the full `build-ralph` test suite
passes. However, the snapshot currently includes any fixed-equatorial body, not
just catalog stars, so fixed-coordinate deep-sky objects can enter the future
star batch path. The public snapshot surface also does not expose the proper
motion, parallax, velocity, or mask arrays as spans, which limits its usefulness
for HP-032B's batch computation. These issues should be fixed before accepting
the task.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Non-star fixed bodies enter star astrometry arrays

Severity: MAJOR
File:
`libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`
Lines/functions: lines 9-12, `isCatalogStarBody`

Problem:
`isCatalogStarBody()` returns true for every body whose `ephemerisSource` is
`FixedEquatorial`, regardless of `body.type`. Catalog normalization sets that
source for any body with fixed coordinates, and OpenNGC and bundled Messier
deep-sky objects are stored with fixed coordinates. As a result, those
deep-sky bodies can be copied into `CatalogStarAstrometryArrays`.

Why it matters:
HP-032A is specifically a catalog-star data layout task. Including fixed
deep-sky objects or other non-star fixed bodies pollutes the star-only snapshot
and can cause HP-032B's batch star propagation to process non-star body indices.
The current test excludes a planet, but it does not cover a fixed-equatorial
deep-sky object or constellation, which is the realistic failure mode.

Recommended fix:
Restrict the predicate so fixed-equatorial fallback is accepted only for star
bodies, or otherwise require an explicit star source/type before adding the
body to the array. Add a regression test with a `DeepSkyObject` that has
`fixedEquatorial` and `FixedEquatorial` source, and assert it is excluded.

### Finding 2: Batch-needed astrometry arrays are not exposed as arrays

Severity: MAJOR
File:
`libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.hpp`
Lines/functions: lines 33-39, public span accessors

Problem:
The class stores cache-friendly value and mask vectors for proper motion,
stellar parallax, radial velocity, fixed-coordinate fallback, and validity
ranges, but the public API exposes spans only for reference RA/Dec and epoch.
The remaining astrometry data is available only through scalar
`std::optional` getters.

Why it matters:
HP-032A exists to provide the immutable array snapshot that HP-032B can use for
high-throughput batch star propagation. A batch calculator cannot iterate over
the proper-motion, parallax, radial-velocity, and availability arrays without
either adding more API later or using per-index optional access, which defeats
the cache-friendly data layout this task is meant to establish.

Recommended fix:
Expose read-only spans for the value arrays and corresponding presence masks
needed by batch propagation, including proper motion RA/Dec, stellar parallax,
radial velocity, fixed fallback coordinates, and validity flags where needed.
Keep the existing scalar helpers if they are useful for tests or callers.

### Finding 3: Presence masks do not match scalar validity semantics

Severity: MINOR
File:
`libs/skygate-ephemeris/src/engine/highprecision/CatalogStarAstrometryArrays.cpp`
Lines/functions: lines 80-96, `maskForOptional`

Problem:
The presence masks are based only on `optional.has_value()`. The existing
`StarAstrometryCalculator` treats non-finite proper motion and radial velocity
values as unavailable, and treats stellar parallax as usable only when finite
and positive for parallax-dependent corrections.

Why it matters:
A future batch consumer using these masks as availability indicators can diverge
from the scalar calculator for programmatic or malformed catalog inputs such as
NaN, infinity, or negative parallax. The source parsers often sanitize values,
but the public `CelestialBody` model does not guarantee that all constructed
catalog astrometry is finite and physically usable.

Recommended fix:
Either make the masks reflect the scalar calculator's validity rules, or rename
and document them as raw source-presence masks and add separate validity masks
for batch propagation. Add tests covering non-finite proper motion or radial
velocity and non-positive parallax.

## Test assessment

The new
`skygate-ephemeris-catalog-star-astrometry-arrays-tests` target covers full
astrometry, partial astrometry, fixed-only stars, array span sizes, lookup by
body index, and source lifetime independence. The missing coverage is exclusion
of non-star fixed-coordinate catalog bodies, span access for all batch-needed
arrays, and invalid numeric astrometry values.

Commands run:

- Built target:
  `skygate-ephemeris-catalog-star-astrometry-arrays-tests`
- Ran focused CTest:
  `skygate-ephemeris-catalog-star-astrometry-arrays-tests`
- Ran full CTest suite in `build-ralph`
- Ran `git diff --check HEAD^..HEAD`

All commands passed. The full CTest run passed 123/123 tests, with the existing
CALCEPH-dependent tests skipped by the high-precision-disabled configuration.

## Regression risk

Medium

The new type is not yet used by the main compute path, so current runtime risk
is limited. The risk becomes meaningful for HP-032B because the snapshot can
already contain non-star catalog bodies and does not expose all batch-needed
arrays through cache-friendly accessors.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
