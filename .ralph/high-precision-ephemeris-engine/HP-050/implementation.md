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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Old public enum spellings were removed instead of compatibility-aliased
  - Action: Fixed
  - Notes: Added guarded deprecated compatibility enumerators so `CatalogSelectionMode::None` maps to `Disabled` and `CatalogLoadErrorCode::None` maps to `NoError` only when no `None` macro is defined. Added macro-clean compile coverage in a separate translation unit while preserving the existing X11-style macro include-order test.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoader.hpp`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoadResult.hpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderLegacyAliasTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-050/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-050/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-public-header-macro-compat-tests skygate-ephemeris-catalog-factory-tests skygate-ephemeris-catalog-property-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(public-header-macro-compat|catalog-factory|catalog-property)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 102/103 passed with the known unrelated `skygate-ui-qml-main-window-tests` failure tracked by HP-051.
- Remaining concerns: The unrelated UI test failure tracked by HP-051 remains.
