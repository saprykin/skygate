## Task
- ID: HP-002
- Title: Add optional high-precision build dependencies and gates

## Status
READY

## Acceptance criteria claimed
- [x] `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS` CMake option added and defaults to `OFF`
- [x] Disabled builds configure and build without CALCEPH or zstd
- [x] Enabled builds require CALCEPH and zstd through dedicated high-precision dependency wiring
- [x] vcpkg dependency configuration includes CALCEPH and zstd behind a high-precision manifest feature
- [x] vcpkg-enabled high-precision core presets added for Linux, Windows, and macOS
- [x] High-precision dependency smoke test added and registered only when high precision is enabled
- [x] Existing tests pass

## Files changed
- `CMakeLists.txt`
- `CMakePresets.json`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionDependencySmokeTests.cpp`
- `vcpkg.json`

## Important notes
- High-precision enabled configure was checked in this container without CALCEPH installed; CMake fails clearly at `find_package(calceph CONFIG REQUIRED GLOBAL)`.
- Added unrelated follow-up task `HP-049` to `IMPLEMENTATION_PLAN.md` because HP-004 is marked complete but the corresponding public API model types are absent from the codebase.
