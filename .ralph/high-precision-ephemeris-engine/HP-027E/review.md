## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027E
- Title: Implement annual parallax handling for apparent-place inputs
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 227f58f
- Head ref: 04e1ae6

## Summary

The implementation adds annual parallax handling to the star astrometry
calculator, threads the CALCEPH kernel provider through factory wiring, and adds
focused unit coverage for success and degradation paths. The core vector math is
plausible, and the full current test suite passes, but the production path uses
the request epoch directly when asking CALCEPH for Earth's barycentric state.
The real CALCEPH provider rejects non-TDB epochs, so normal UTC/TT requests will
degrade instead of applying annual parallax even when kernel data is available.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Annual parallax uses non-TDB epochs with CALCEPH

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: lines 376-393, `StarAstrometryCalculator::calculate`

Problem:
When annual parallax is requested, the star calculator passes
`input.request.epoch` directly to
`m_kernelProvider->computeGeometricState(...)`. The production
`CalcephKernelProvider` rejects every epoch whose scale is not `TimeScale::Tdb`.
The high-precision compatibility request path constructs UTC epochs, and the new
unit test helper creates TT epochs, so the current tests do not exercise the
real provider contract.

Why it matters:
Annual parallax will be reported as unavailable for normal UTC/TT requests even
when CALCEPH kernel data is wired and valid. That means the main acceptance
criterion, applying annual parallax when catalog parallax metadata and Earth
barycentric state are available, is not met for the production provider path.

Recommended fix:
Convert the request epoch to TDB before calling the kernel provider, using the
existing time-scale service wiring or another existing high-precision helper
pattern. Add a test with a provider that enforces `TimeScale::Tdb`, preferably
through the engine/factory path or by injecting the conversion dependency into
the star calculator.

## Test assessment

Focused `StarAstrometryCalculatorTests` were added for annual-parallax success,
missing kernel provider, and missing source parallax. These cover the basic
branches but not the real CALCEPH epoch-scale precondition. The relevant
high-precision CTest subset passed. The full `ctest --test-dir build-ralph
--output-on-failure` run passed 122/122 tests, with
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests` skipped by the current
build configuration.

## Regression risk

Medium

The changed path is isolated to catalog-star astrometry, but it is part of the
high-precision apparent-place pipeline and now depends on CALCEPH provider
semantics.

## Out-of-scope observations

None.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
