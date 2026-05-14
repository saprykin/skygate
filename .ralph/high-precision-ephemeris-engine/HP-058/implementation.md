## Task
- ID: HP-058
- Title: Reproduce build-ralph footer popup toolbar toggle QML failure

## Status
READY

## Acceptance criteria claimed
- [x] Reproduced the `build-ralph` QML main-window failure
- [x] Investigated the footer popup and timeline toolbar toggle state path
- [x] Fixed the test harness so the footer popup toolbar toggle click starts from the expected expanded timeline toolbar state
- [x] Ran `clang-format` on the touched C++ test file
- [x] Built all targets in `build-ralph`
- [x] Ran `ctest --test-dir build-ralph --output-on-failure`

## Files changed
- `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-058/implementation.md`

## Important notes
- The failure was caused by `Main.qml` collapsing the timeline toolbar at the default `1100x760` test window size when expanded top toolbars overlap. The footer popup test now uses a wide viewport and explicitly expands both toolbars before exercising popup-closing toggle clicks.
- Full-suite verification passed: 120/120 tests passed; the two CALCEPH-dependent tests were skipped by the configured build.
