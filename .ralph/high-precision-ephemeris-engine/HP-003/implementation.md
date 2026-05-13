## Task
- ID: HP-003
- Title: Choose and wire the ERFA/SOFA strategy

## Status
READY

## Acceptance criteria claimed
- [x] ERFA integration route selected as repo-local vcpkg overlay port, with system-package fallback through `FindERFA.cmake`
- [x] ERFA dependency wired behind `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`
- [x] Public code calls ERFA through an internal high-precision wrapper
- [x] High-precision-only ERFA smoke test added for a known calendar-to-Julian-date routine
- [x] Simple-only build compiles with no ERFA dependency
- [x] Existing core and ephemeris tests pass

## Files changed
- `CMakeLists.txt`
- `cmake/FindERFA.cmake`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionErfaSmokeTests.cpp`
- `vcpkg.json`
- `vcpkg-configuration.json`
- `vcpkg/ports/erfa/portfile.cmake`
- `vcpkg/ports/erfa/vcpkg.json`

## Important notes
- `build-ralph` was configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`; `skygate-ephemeris` built and all 44 configured core/ephemeris tests passed.
- The container did not provide vcpkg, CALCEPH, zstd, or ERFA development packages. For high-precision smoke verification only, `build-ralph-highprecision` used a temporary local prefix with actual ERFA 2.0.1 built from the pinned upstream release and minimal CALCEPH/zstd shims to satisfy pre-existing HP-002 configure gates. The new ERFA smoke test passed there.
