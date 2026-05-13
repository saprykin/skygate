## Task
- ID: HP-050
- Title: Rename catalog public `None` enumerators to avoid macro collisions

## Status
READY

## Acceptance criteria claimed
- [x] Public catalog headers audited for `None` enumerators.
- [x] `CatalogSelectionMode::None` renamed to `CatalogSelectionMode::Disabled`.
- [x] `CatalogLoadErrorCode::None` renamed to `CatalogLoadErrorCode::NoError`.
- [x] Public include-order coverage now includes catalog headers after an X11-style `None` macro definition.
- [x] In-repo callers updated to the renamed enumerators.
- [x] Touched C++ files formatted with `clang-format`.
- [x] Relevant catalog/API tests pass.

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoader.hpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoadResult.hpp`
- `libs/skygate-ephemeris/src/catalog/model/CatalogBodyParseResult.hpp`
- `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
- `libs/skygate-ephemeris/tests/catalog/CatalogFactoryTests.cpp`
- `libs/skygate-ephemeris/tests/catalog/CatalogPropertyTests.cpp`
- `apps/skygate-ui/src/catalog/CatalogCoordinator.cpp`

## Important notes
- Built targeted ephemeris catalog/API targets in `build-ralph`.
- Reconfigured `build-ralph` with `SKYGATE_BUILD_UI=ON` and built `skygate-ui` plus all default targets.
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(public-header-macro-compat|catalog-factory|catalog-property)-tests'` passed.
- Full `ctest --test-dir build-ralph --output-on-failure` passed 102 of 103 tests. The persistent failure is unrelated to HP-050: `skygate-ui-qml-main-window-tests` fails in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.
