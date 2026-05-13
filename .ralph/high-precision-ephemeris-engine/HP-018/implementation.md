## Task
- ID: HP-018
- Title: Define ephemeris data manifest schema and parser

## Status
READY

## Acceptance criteria claimed
- [x] Manifest model includes kernels, leap-second data, EOP data, Delta T data, source URLs, versions, checksums, validity ranges, compression metadata, and profile names
- [x] Manifest profiles distinguish bundled modern data from optional DE441 long-range data
- [x] Parser validates JSON manifest structure without constructing ephemeris results
- [x] Tests added for valid manifests, missing required fields, checksum metadata, validity-range parsing, compression metadata, modern profile, and optional DE441 profile
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataManifest.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataManifest.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisDataManifestTests.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`

## Important notes
- Verified with `cmake --preset core-debug -B build-ralph`, `cmake --build build-ralph -j2`, and `ctest --test-dir build-ralph --output-on-failure`.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Duplicate manifest IDs are accepted
    - Action: Fixed
    - Notes: Added validation for duplicate profile IDs, duplicate asset IDs, and repeated asset IDs inside a profile.
  - Boolean fields are silently coerced
    - Action: Fixed
    - Notes: Added explicit JSON boolean validation for required profile `bundled` and `longRange` fields and optional asset `optional` fields.
  - Large compression sizes can overflow during parsing
    - Action: Fixed
    - Notes: Compression size fields now reject values outside `uint64_t` and values above the exact JSON integer range accepted by the parser.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataManifest.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/EphemerisDataManifestTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-018/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-018/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-data-manifest-tests -j2` PASS
  - `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-data-manifest-tests` PASS
  - `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-data-manifest-tests` PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS
- Remaining concerns: None.
