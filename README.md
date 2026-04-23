# SkyGate

SkyGate is a Qt 6 desktop application for exploring an interactive sky view.
It combines reusable projection math and ephemeris libraries with a Qt Quick UI
that renders stars, constellations, the horizon, and overlay labels.

## Highlights

- Interactive sky map with pan, zoom, hover, and label overlays
- Bundled Messier deep-sky object markers with symbolic glyph rendering
- Time playback controls with pause, step, and speed adjustment
- Fixed UTC popup accepts BCE/BC dates using proleptic Gregorian years
- Multiple projections: `Stereographic`,
  `AzimuthalEquidistant`, and `Perspective`
- Observer location from current device, bundled city catalog, or
  custom coordinates
- Bundled starter catalog plus support for downloading HYG v4.2 stars,
  OpenNGC deep-sky objects, and Stellarium constellation data
- Persistent settings and cached catalog data between launches

Ancient dates are intended for exploratory viewing. The date input uses a
proleptic Gregorian calendar, and the current Sun, Moon, and planet formulas
are lightweight approximations around J2000 rather than historical-astronomy
grade ephemerides for the far past.

## Repository Layout

- `apps/skygate-ui`
  Qt Quick application shell, QML UI, scene graph rendering,
  settings, and catalog workflow
- `libs/skygate-core`
  Projection contracts, math helpers, viewport logic, and time abstractions
- `libs/skygate-ephemeris`
  Catalog parsing, celestial coordinate calculations, and sky
  snapshot generation
- `docs/ARCHITECTURE.md`
  Detailed architecture notes, runtime flow, and extension points

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- Qt 6.5 or newer with `Core`, `Gui`, `Qml`, `Quick`, `Network`, and `Test`
- `Qt Positioning` (optional, enables the `Current Device` location mode)
- Zlib

## Build

SkyGate uses standard CMake. If CMake cannot find Qt automatically, point
`CMAKE_PREFIX_PATH` at your Qt installation root such as
`/path/to/Qt/6.x/<platform>`.

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSKYGATE_BUILD_UI=ON \
  -DSKYGATE_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<platform>

cmake --build build
```

To build the libraries and tests without the UI application:

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSKYGATE_BUILD_UI=OFF \
  -DSKYGATE_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<platform>

cmake --build build-core
```

## Run

With the UI enabled, the simplest way to launch the application is:

```bash
cmake --build build --target run-skygate-ui
```

On macOS, the build also produces an app bundle at:

```text
build/apps/skygate-ui/SkyGate.app
```

There is also a macOS convenience target that opens the bundle:

```bash
cmake --build build --target run-skygate-ui-bundle
```

On non-bundle platforms, the executable target is `skygate-ui` inside the build
tree.

## Test

All module tests are registered with CTest:

```bash
ctest --test-dir build --output-on-failure
```

This covers core projection and math tests, ephemeris and catalog parsing
tests, and UI/controller/model tests.

## CMake Presets

The repository includes `CMakePresets.json` with `core-debug`, `ui-debug`, and
`ui-run` presets. They keep a stable local workflow and use the existing
`build-make/<preset>` layout.

The shared preset file is portable and does not check in developer-specific Qt
or macOS SDK paths. For UI builds, make Qt discoverable through your local
environment before running `cmake --preset ui-debug`. A common macOS setup is:

```bash
export CMAKE_PREFIX_PATH="$HOME/Qt/6.x/macos"
cmake --preset ui-debug
```

If your machine needs a specific SDK override, set `CMAKE_OSX_SYSROOT` locally
in your shell or IDE configuration rather than editing the tracked presets.
`CMakeUserPresets.json` is also supported for local-only overrides and should
not be checked in.

## Packaging

On macOS, a DMG packaging target is available when `macdeployqt` is installed
and discoverable:

```bash
cmake --build build --target package-skygate-ui-dmg
```

## Notes

- The app works with the bundled catalog out of the box; network access is only
  required for external catalog and constellation downloads.
- Downloaded/imported star and deep-sky catalogs are cached across launches via
  `QSettings` and app-data cache files.
- Bundled Messier deep-sky object data and the downloadable OpenNGC preset use
  OpenNGC v20260307 by Mattia Verga, licensed under CC-BY-SA-4.0:
  https://github.com/mattiaverga/OpenNGC
- For a deeper code tour, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
