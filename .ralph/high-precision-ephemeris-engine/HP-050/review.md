## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-050
- Title: Rename catalog public `None` enumerators to avoid macro collisions
- Source: IMPLEMENTATION_PLAN.md, HP-050
- Base ref: a5aba1886d3d6cb3ea55822a1af176c5e6506995
- Head ref: 168c138564350e497204545c4d2d1010282d5542

## Summary

The implementation renames the public catalog `None` enumerators to
`Disabled` and `NoError`, updates in-repo callers, and adds compile coverage for
including the catalog public headers after an X11-style `None` macro. The macro
collision goal is met, but the old public enum spellings were removed outright,
despite the task requiring source compatibility where practical.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Old public enum spellings were removed instead of compatibility-aliased

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoader.hpp`, `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoadResult.hpp`
Lines/functions: `CatalogSelectionMode` lines 14-17; `CatalogLoadErrorCode` lines 20-30

Problem:
The task asks to rename `CatalogSelectionMode::None` and
`CatalogLoadErrorCode::None` while preserving source compatibility where
practical. The implementation removes both old public spellings entirely.
Downstream code that includes these headers in an environment where `None` is
not macro-defined now fails to compile.

Why it matters:
These are public header APIs. The rename fixes X11 macro compatibility, but it
also creates an avoidable source break for existing callers that used the old
enum values in non-X11 or macro-clean include contexts.

Recommended fix:
Add deprecated compatibility enumerator aliases guarded by `#ifndef None`, for
example mapping `CatalogSelectionMode::None` to `Disabled` and
`CatalogLoadErrorCode::None` to `NoError` only when the `None` macro is not
defined. Keep the existing macro-compatibility test to ensure headers still
compile when `None` is defined, and add a small compile/API assertion for the
legacy aliases in a macro-clean translation unit.

## Test assessment

Relevant targeted targets build successfully in `build-ralph`:
`skygate-ephemeris-public-header-macro-compat-tests`,
`skygate-ephemeris-catalog-factory-tests`, and
`skygate-ephemeris-catalog-property-tests`.

Targeted CTest also passes:
`ctest --test-dir build-ralph --output-on-failure -R
'skygate-ephemeris-(public-header-macro-compat|catalog-factory|catalog-property)-tests'`
passed 3/3 tests.

The new macro include-order coverage is meaningful: it defines `None` before
including `CatalogLoadResult.hpp` and `CatalogLoader.hpp`, then references the
new enum values. What is missing is compatibility coverage for the old enum
spellings in a translation unit where `None` is not defined.

Full CTest was also run. It reproduced the implementation handoff result:
102/103 tests passed, with only the unrelated
`skygate-ui-qml-main-window-tests` failure at
`apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Regression risk

Medium

The code change is mechanically narrow and in-repo callers were updated, but
removing public enum spellings can break external or future in-tree callers that
still use the previous API names.

## Out-of-scope observations

- The persistent `skygate-ui-qml-main-window-tests` failure is already tracked
  by HP-051 and does not appear related to HP-050.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
