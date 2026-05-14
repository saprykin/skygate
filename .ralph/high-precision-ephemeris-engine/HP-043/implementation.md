## Task
- ID: HP-043
- Title: Add Preferences engine selector and option controls

## Status
READY

## Acceptance criteria claimed
- [x] Preferences exposes exactly two top-level engine choices.
- [x] High-precision correction presets are wired through the draft and
  controller settings path.
- [x] Refraction and atmosphere controls are shown only for high precision.
- [x] Draft reset/apply and saved setting restore cover ephemeris settings.
- [x] Malformed saved ephemeris settings fall back to defaults.
- [x] Existing tests pass.

## Files changed
- `apps/skygate-ui/qml/preferences/PreferencesCatalogSection.qml`
- `apps/skygate-ui/qml/preferences/PreferencesDraft.qml`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
- `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`
- `apps/skygate-ui/tests/qml/QmlPreferencesDraftTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed: 125/125
  tests passed. Existing CALCEPH kernel-provider and solar-system calculator
  tests were skipped by the current build configuration.
