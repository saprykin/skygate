## Task fixed
  - ID: HP-005
  - Title: Add public result status and provenance model
  - Source: `IMPLEMENTATION_PLAN.md`; `spec/high-precision-ephemeris-engine.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-005/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-005/implementation.md`

## Summary
  Reworked the per-state metadata representation so snapshot states no longer carry allocation-prone warning vectors, owned provenance strings, or copied validity-range strings. Metadata still exposes status, warning codes with display text, provenance, effective validity range, uncertainty, and applied corrections.

## Findings addressed
  - Finding title: Heavy metadata is embedded in every per-frame body state
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`; `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`; `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`; `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
  - What changed: `EphemerisResultMetadata` now stores warnings as a compact bitmask, provenance as `std::string_view`, and effective validity range as a shared pointer. `EphemerisWarning` now derives display text on demand from its stable code. Simple-engine state creation uses static provenance and warning helpers instead of per-state vector/string writes. Tests were updated to verify the compact accessors and enforce a small metadata size.
  - Why this resolves the finding: The hot `CelestialBodyState` path keeps status/provenance/warning information readable without heap-backed vectors or strings per body, removing the allocation and memory-bandwidth regression called out by review.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-fallback)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(engine-baseline|engine-fallback|regression)-tests'`: PASS
  - `cmake --build build-ralph -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 102/103 passed with the known unrelated HP-051 `skygate-ui-qml-main-window-tests` failure.

## Files changed
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-005/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-005/fix.md`

## Remaining concerns
  Full CTest still reports the known unrelated HP-051 QML main-window failure.

## Final fixer status
  READY_FOR_REVIEW
