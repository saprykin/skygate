## Task fixed
  - ID: HP-044
  - Title: Add Preferences ephemeris data controls
  - Source: `IMPLEMENTATION_PLAN.md` HP-044

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-044/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-044/implementation.md`

## Summary
  Fixed the missing Preferences path for installing optional DE441 long-range
  data. Preferences now exposes separate modern and DE441 update actions, and
  the controller can activate an explicit ephemeris data profile through
  `SkyEphemerisDataManager`.

## Findings addressed
  - Finding title: DE441 update path is not reachable from Preferences
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/qml/preferences/PreferencesCatalogSection.qml`,
    `apps/skygate-ui/src/app/SkyContextController.cpp`,
    `apps/skygate-ui/src/app/SkyContextController.hpp`,
    `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`
  - What changed: Added `updateEphemerisDataProfile(QString)` on the
    controller, made the existing update path delegate to it, added a
    Preferences `Install DE441` action targeting `de441-long-range`, and
    extended QML coverage to invoke the DE441 update path against a staged
    test manifest asset.
  - Why this resolves the finding: Users can now install the optional DE441
    long-range profile directly from Preferences instead of only seeing its
    absent/installed status.

## Tests run
  - `clang-format -i apps/skygate-ui/src/app/SkyContextController.hpp
    apps/skygate-ui/src/app/SkyContextController.cpp
    apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`: PASS
  - `cmake --build build-ralph --target
    skygate-ui-qml-preferences-catalog-tests -j2`: PASS
  - `QT_QPA_PLATFORM=offscreen
    build-ralph/apps/skygate-ui/tests/skygate-ui-qml-preferences-catalog-tests
    -platform offscreen`: PASS
  - Focused 7-test manager/controller/preferences CTest regex: PASS
    ```sh
    QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph -R \
      "skygate-ui-(qml-preferences-catalog-tests|"\
      "qml-preferences-draft-tests|"\
      "qml-preferences-window-tests|"\
      "sky-ephemeris-data-manager-tests|"\
      "context-controller-ephemeris-settings-tests|"\
      "settings-store-tests|"\
      "sky-settings-codecs-tests)" \
      --output-on-failure
    ```
  - `cmake --build build-ralph -j2`: PASS
  - `QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph
    --output-on-failure`: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-044/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-044/implementation.md`
  - `apps/skygate-ui/qml/preferences/PreferencesCatalogSection.qml`
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/app/SkyContextController.hpp`
  - `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
