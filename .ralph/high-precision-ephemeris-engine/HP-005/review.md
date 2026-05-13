## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-005
- Title: Add public result status and provenance model
- Source: `IMPLEMENTATION_PLAN.md`; `spec/high-precision-ephemeris-engine.md`
- Base ref: `c0def176d0e9049cd2289d32441a7401d97566f7`
- Head ref: `803eaeaca94adb58c67fdfec4c3b4df5620fd3d1`

## Summary

The implementation adds public result status, warning, and metadata models to
the ephemeris API, extends `CelestialBodyState` with metadata while keeping the
existing coordinate fields readable, and populates simple-engine metadata for
valid, degraded, and unsupported compatibility states. The model is functionally
close, but the chosen representation makes every per-frame body state carry
heavy, allocation-prone metadata and regresses the existing snapshot hot path.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Heavy metadata is embedded in every per-frame body state

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`; `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: `CelestialBodyState`, `EphemerisResultMetadata`, `SimpleEphemerisEngine::computeStateForBody`

Problem:
`CelestialBodyState` is the hot per-frame state vector used by rendering,
search, trails, and tests. HP-005 adds `EphemerisResultMetadata` directly to
each state, and that metadata contains `std::vector`, `std::string`,
`std::optional<EphemerisDateRange>`, and another `std::string`-heavy range
model. The simple engine then writes `"Simple ephemeris engine"` into every
state during snapshot computation.

Why it matters:
The previous state shape was just body index plus coordinates. The new state is
substantially larger, and the simple-engine provenance string is long enough to
require per-state heap allocation with typical standard-library string
implementations. For large catalogs this can add thousands of allocations and a
large memory-bandwidth penalty on every scene recompute, which conflicts with
the existing lightweight snapshot design and the spec's requirement to optimize
full-frame rendering/search across large object sets.

Recommended fix:
Keep the per-frame `CelestialBodyState` metadata representation lightweight.
For example, split heavy provenance/range/warning text into shared snapshot or
dataset-level tables and store compact IDs/status/flags in each state, or add a
richer result type for detailed single-object/request results while preserving
a lightweight adapter for `SkySnapshot` consumers. Avoid assigning heap-backed
provenance strings per body in the simple-engine compute loop.

## Test assessment

Relevant HP-005 tests exist in
`libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp` and
`libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`. They
cover all result status values, representative warning text, metadata defaults,
metadata field readability on `CelestialBodyState`, and simple-engine metadata
for successful, degraded, and unsupported states. They do not cover the
performance and allocation impact of embedding heavy metadata in every snapshot
state.

Commands run:

- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests -j2`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-fallback)-tests'`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(engine-baseline|engine-fallback|regression)-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

The targeted HP-005 and ephemeris regression tests passed. Full CTest ran 103
tests with 102 passing and one failure in
`skygate-ui-qml-main-window-tests`, matching the known unrelated HP-051 failure.

## Regression risk

Medium

Existing coordinate fields and lookup behavior remain compatible, but the
metadata representation affects the size and allocation behavior of every
snapshot state produced by the engine.

## Out-of-scope observations

- If warning codes become serialized or persisted later, consider assigning
  explicit numeric values to `EphemerisWarningCode` before that persistence
  boundary is introduced.
- Future high-precision producer tasks should add behavioral tests for
  out-of-range and failed computation results when those paths exist.
- `ctest --test-dir build-ralph --output-on-failure` still fails
  `skygate-ui-qml-main-window-tests`, already tracked separately as HP-051.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
