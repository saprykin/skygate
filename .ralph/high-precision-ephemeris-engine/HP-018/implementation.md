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
