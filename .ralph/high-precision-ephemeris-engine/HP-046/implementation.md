## Task
- ID: HP-046
- Title: Enable high precision for release builds while preserving simple-only
  developer builds

## Status
READY

## Acceptance criteria claimed
- [x] Release vcpkg presets enable high precision and the
  `high-precision-ephemeris` manifest feature.
- [x] Linux AppImage packaging enables high precision by default and uses vcpkg
  when `VCPKG_ROOT` is available.
- [x] Package workflows provision vcpkg for high-precision release dependency
  resolution.
- [x] Developer simple-only builds remain explicit through
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
- [x] High-precision test presets are available for vcpkg-backed builds.
- [x] Existing simple-only build and tests pass in `build-ralph`.

## Files changed
- `CMakePresets.json`
- `.github/workflows/package-linux.yml`
- `.github/workflows/package-macos.yml`
- `.github/workflows/package-windows.yml`
- `packaging/linux/build-appimage.sh`
- `README.md`
- `docs/RELEASE.md`
- `.ralph/high-precision-ephemeris-engine/HP-046/implementation.md`

## Important notes
- `cmake --list-presets=all` validates the updated preset file.
- A high-precision release configure in this container reached the expected
  dependency gate and failed because CALCEPH is not installed locally.
- `cmake -S . -B build-ralph -DCMAKE_BUILD_TYPE=Debug -DSKYGATE_BUILD_UI=ON
  -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`
  configured successfully.
- `cmake --build build-ralph --parallel` hit the container memory limit while
  compiling generated QML cache sources; rerunning with `--parallel 2` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed: 125 tests run,
  123 passed, 2 skipped high-precision CALCEPH kernel tests in simple-only
  mode.
