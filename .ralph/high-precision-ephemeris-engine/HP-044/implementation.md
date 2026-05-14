## Task
- ID: HP-044
- Title: Add Preferences ephemeris data controls

## Status
READY

## Acceptance criteria claimed
- [x] Preferences Catalog page includes an `Ephemeris Data` group.
- [x] Modern kernel, DE441 long-range, EOP, leap-second, Delta T, and
  last-update statuses are exposed to QML.
- [x] Online/offline update mode and update-button enabled state are exposed.
- [x] Clear ephemeris data cache action is routed through
  `SkyEphemerisDataManager`.
- [x] QML tests cover fallback display, installed DE441 display, update-mode
  enabled states, clear-cache invocation, and no QML warnings.
- [x] Existing tests pass.

## Files changed
- `apps/skygate-ui/qml/preferences/PreferencesCatalogSection.qml`
- `apps/skygate-ui/qml/windows/PreferencesWindow.qml`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
- `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`
- `apps/skygate-ui/tests/qml/QmlPreferencesWindowTests.cpp`

## Verification
- `clang-format` on touched C++ source and header files.
- Focused catalog preferences build:
  ```sh
  cmake --build build-ralph \
    --target skygate-ui-qml-preferences-catalog-tests -j2
  ```
- Focused catalog preferences executable:
  ```sh
  QT_QPA_PLATFORM=offscreen \
    build-ralph/apps/skygate-ui/tests/skygate-ui-qml-preferences-catalog-tests \
    -platform offscreen
  ```
- Focused manager/controller/preferences CTest regex:
  ```sh
  QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph \
    -R '<ephemeris-manager-controller-and-preferences-tests>' \
    --output-on-failure
  ```
- `cmake --build build-ralph -j2`
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph --output-on-failure`

## Important notes
- Full CTest passed 125 of 125 run tests. The existing CALCEPH-dependent
  tests `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests` were skipped by the
  current build configuration.
