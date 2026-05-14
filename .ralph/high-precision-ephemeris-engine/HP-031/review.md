## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-031
- Title: Implement single-star astrometry propagation
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: fadc3d7d94641e63d3b33d0bcfd24f4472361c28
- Head ref: f2744c6af17753060937e45f6082ca3894d2b31f

## Summary

The implementation adds `CatalogStarAstrometry`, HYG astrometry parsing, a
single-star propagation calculator, factory wiring, and focused tests. The
overall shape matches HP-031, but the calculator does not fully respect the
stellar parallax correction flag, the HYG `pmra` path likely applies the
declination cosine twice, and HYG missing-distance sentinels can be treated as
valid parallax. Those are correctness issues in the core task.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Stellar parallax flag is ignored during propagation

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: 194-215, `propagatedEquatorial`

Problem:
`propagatedEquatorial()` switches to distance-based propagation whenever the
catalog star has a positive parallax. That happens even when
`EphemerisCorrectionFlags::StellarParallax` is not requested. With
`ProperMotion | RadialVelocity`, but without `StellarParallax`, the code still
uses `stellarParallaxMas` to compute distance and perspective effects.

Why it matters:
HP-031 requires the calculator to respect flags controlling proper motion,
radial velocity, and stellar parallax. Callers that disable stellar parallax can
still get parallax-dependent results, while metadata does not report
`StellarParallax` as applied. That makes the result numerically and
diagnostically inconsistent.

Recommended fix:
Only use catalog parallax when `StellarParallax` is requested. If radial
velocity is requested without an enabled positive parallax, either skip radial
velocity and mark `CorrectionUnavailable`, or document and test a different
explicit policy.

### Finding 2: Catalog RA proper motion is likely scaled by cos(dec) twice

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: 190-192, `propagatedEquatorial`

Problem:
The calculator multiplies `properMotionRightAscensionMasPerYear` by
`cos(declination)` before applying it along the east tangent basis. The HYG
parser directly fills that field from `pmra`, and the common catalog convention
for `pmra` is the tangent-plane RA component, `mu_alpha * cos(delta)`, in
mas/year. Applying another `cos(delta)` under-propagates RA motion for stars
away from the equator. The current fixture uses a coordinate RA delta, so it
does not catch this.

Why it matters:
This task is specifically about single-star astrometry propagation. Incorrect
RA proper-motion semantics will produce systematically wrong propagated star
positions for real catalog data, with error increasing at high declination.

Recommended fix:
Make the public payload semantics explicit and align parser plus calculator.
Either store tangent-plane `pmra` and remove the extra `cos(delta)`, or convert
HYG `pmra` to coordinate RA motion in the parser and rename/document the field
accordingly. Add a non-equatorial fixture using catalog-style `pmra` so the
behavior is locked down.

### Finding 3: HYG missing-distance sentinel becomes valid parallax

Severity: MAJOR
File: `libs/skygate-ephemeris/src/catalog/hyg/HygCatalogParser.cpp`
Lines/functions: 57-60, `parallaxMasFromHygRow`

Problem:
When explicit `parallax`/`plx` is absent, the parser converts any positive
`dist` value to milliarcsecond parallax. HYG uses very large distances such as
`100000` parsecs for missing or dubious parallax data, but this code converts
that sentinel to a positive `0.01` mas parallax.

Why it matters:
Unavailable distance data can look like valid stellar parallax. The calculator
can then use it for parallax/radial-velocity availability and mark related
corrections as applied instead of degraded, contrary to HP-031's requirement to
return degraded status when required astrometry fields are missing.

Recommended fix:
Treat HYG missing-distance sentinels as absent astrometry when deriving
parallax from `dist`. Add parser coverage for the sentinel case and verify that
requests for parallax or radial velocity degrade instead of using the sentinel.

## Test assessment

Focused coverage was added for full astrometry, disabled corrections, partial
astrometry, fixed-only fallback, invalid coordinate failure, HYG column parsing,
and API model construction. The tests do not cover disabling only stellar
parallax while radial velocity or proper motion remain enabled, and they do not
cover catalog-style `pmra` semantics at non-zero declination or HYG missing
distance sentinels.

I ran:

- `cmake --build build-ralph --target
  skygate-ephemeris-star-astrometry-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R` for the focused
  star-astrometry, HYG catalog, and API model tests
- `ctest --test-dir build-ralph --output-on-failure`

All 122 tests passed in the current `build-ralph` configuration. The two
CALCEPH-only tests were skipped because this build has
`SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.

## Regression risk

Medium

The changes are mostly isolated to high-precision star astrometry and HYG
catalog parsing, but the affected code sits on the future engine path for all
catalog stars. The likely errors are numerical rather than build-time failures.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
