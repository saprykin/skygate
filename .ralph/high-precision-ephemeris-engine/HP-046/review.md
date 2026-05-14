## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-046
- Title: Enable high precision for release builds while preserving simple-only
  developer builds
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 11fb9ceed6fc4f15408e5769fe7a9eac7084022d
- Head ref: cb0b739ab52f3a14c2b4319e62c8b46d7b0c7a3c

## Summary

The implementation enables high precision for release vcpkg presets and
package workflows, adds Linux AppImage high-precision configuration, and adds
vcpkg-backed high-precision test presets. The release path looks consistent,
and the simple-only build still works, but the task explicitly requires
developer/simple-only builds to remain explicit through
`SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`. The simple-only developer
presets and plain README examples still rely on the CMake default instead of
setting the flag directly.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Simple-only developer builds are not explicit

Severity: MAJOR
File: `CMakePresets.json`, `README.md`
Lines/functions: `CMakePresets.json:91`, `CMakePresets.json:137`,
`CMakePresets.json:157`, `CMakePresets.json:187`,
`CMakePresets.json:219`, `CMakePresets.json:249`, `README.md:82`,
`README.md:95`

Problem:
HP-046 requires developer/simple-only builds to remain explicit and documented
through `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`. The simple-only
developer presets omit that cache variable and rely on the global CMake default
of `OFF`. The plain README examples are described as custom simple-only setups,
but they also omit the flag.

Why it matters:
This leaves one of the task acceptance criteria only partially satisfied. It
also makes the build policy harder to audit because release presets explicitly
turn high precision on, while simple-only developer presets and examples do not
explicitly turn it off.

Recommended fix:
Add `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` to the simple-only developer
configure presets, including `core-debug`, `ui-debug`, and the non-
`highprecision` vcpkg debug presets. Update the plain README CMake examples to
pass `-DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, and keep the AppImage
opt-out text tied clearly to that CMake flag.

## Test assessment

The implementation adds high-precision vcpkg test presets for Linux, Windows,
and macOS. I validated that `cmake --list-presets=all` parses the preset file
and exposes the Linux high-precision test preset in this environment.

I configured a simple-only UI/test build in `build-ralph` with
`SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, built it with
`cmake --build build-ralph --parallel 2`, and ran
`ctest --test-dir build-ralph --output-on-failure`. All 125 tests passed, with
the two CALCEPH-backed kernel tests skipped as expected in simple-only mode.

I did not run high-precision vcpkg tests locally because `VCPKG_ROOT` is not
set in this container, and the prompt restricts testing to the existing
`build-ralph` directory.

## Regression risk

Low

The release high-precision path is configuration-only and appears consistent
with existing vcpkg manifest features. The remaining issue is a build-policy and
documentation gap, not a runtime behavior regression.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
