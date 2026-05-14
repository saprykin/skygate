## Task
- ID: HP-033
- Title: Add high-precision computation cache and read-only thread safety

## Status
READY

## Acceptance criteria claimed
- [x] Computation cache added for repeated full-frame high-precision requests
- [x] Factory wires high-precision engines with a default computation cache
- [x] Cached results are isolated by request, catalog, and data-set revision
- [x] Concurrent read-only compute coverage added
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- Verification passed with `ctest --test-dir build-ralph --output-on-failure`.
- CALCEPH kernel smoke tests were skipped by their existing test conditions.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Cache stores snapshots, not shared per-request state
    - Action: Fixed
    - Notes: Added immutable prepared request state caching and passed the
      prepared state through full-frame, indexed, and id-based body paths.
  - Catalog isolation is based on pointer identity and size
    - Action: Fixed
    - Notes: Cache keys now include catalog body content instead of vector
      storage identity.
  - Dataset isolation omits effective range contents
    - Action: Fixed
    - Notes: Cache keys now include dataset display name and every date range
      id, display name, start epoch, and end epoch.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-033/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-033/fix.md`
- Tests run after fix:
  - Build high-precision engine tests: PASS
    - Command:
      `cmake --build build-ralph --target
      skygate-ephemeris-highprecision-engine-tests -j2`
  - Focused high-precision CTest run: PASS
    - Command:
      `ctest --test-dir build-ralph --output-on-failure -R
      <focused high-precision test regex>`
  - Full build: PASS
    - Command: `cmake --build build-ralph -j2`
  - Full CTest suite: PASS
    - Command: `ctest --test-dir build-ralph --output-on-failure`
- Remaining concerns: None.
