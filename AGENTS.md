# AGENTS.md

## Purpose

This file defines repository-specific rules for coding agents working in this
repository.

## Repository Structure

- Co-locate `.hpp` headers alongside their corresponding `.cpp` files under
  `src/` in every module.
- The `src/` directory serves as the public include root for library targets:
  `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)`.
- Do not create a separate `include/` tree.
- Prefer `.hpp` headers and `#pragma once`.
- Use one enum, class, or struct per header/source pair.
- Do not put multiple standalone struct, class, or enum definitions in the same
  header file.
- Nest a type inside another class only when it is tightly coupled and has no
  standalone meaning outside that class.
- If only the definition of a struct, class, or enum is required, still add the
  corresponding `.cpp` file and include the header. Do not add anything else to
  this companion source file.
- Exception: pure interface classes are header-only. Do not add a companion
  `.cpp` file for an interface that contains only virtual methods and no
  implementation.

## Header Rules

- Header files must not contain free functions.
- Header files must not contain namespace-level helper functions.
- If reusable helper behavior is required, create a focused class or struct and
  expose the helper as a public static member.
- If the helper is only needed by one `.cpp` file, keep it in that `.cpp` file
  inside an unnamed namespace.
- Do not use header helpers as a shortcut around proper ownership or
  responsibility boundaries.

## Naming Conventions

- Types/classes: `PascalCase`.
- Interface classes: prefix with `I`, for example `IRenderer` or
  `IProjection`.
- Methods/functions: `camelCase`.
- Member variables: `m_camelCase`.
- Local variables/parameters: `camelCase`.
- Constants: `kPascalCase`.
- Avoid macros unless necessary.
- Do not create aliases for project namespaces.
- Do not use partially qualified project namespaces such as `core::Type` or
  `ephemeris::Type`. Use the full `skygate::...::Type` name, or use an
  explicit `using namespace skygate::...` or `using skygate::...::Type`
  declaration and then refer to the type unqualified.
- Setters are prefixed with `set`, for example `setLatitude(double latitude)`.
- Getters use the property name directly, for example `latitude() const`.
- Boolean getters may use `is`/`has` when clearer, for example
  `isPaused() const`.

## Include Order

Group `#include` directives into three blocks, each separated by a blank line:

1. Current project includes using `"..."`.
2. Non-project library includes, for example Qt `<Q...>`.
3. System/compiler includes, for example `<utility>`.

Rules:

- Do not place blank lines inside an include block.
- In a `.cpp` file, the corresponding header must be the first include in the
  first block.
- Sort the rest of each block alphabetically.
- If includes with different nested levels are used, includes with lower nested
  level come first.

## Qt and CMake Expectations

- Target Qt 6.5+ and C++20.
- Use CMake for all build configuration.
- Prefer the existing CMake presets and build targets when possible:
  `core-debug`, `ui-debug`, and `ui-run`.
- Preserve the one-way dependency direction:
  `skygate-ui -> skygate-ephemeris -> skygate-core`.
- Keep platform-specific code isolated behind interfaces.
- Favor Qt abstractions when they preserve portability.
- For QML-facing types, follow the established `QObject`, `Q_PROPERTY`, and
  `Q_INVOKABLE` pattern instead of inventing alternate UI binding layers.

## Core Library Usage

- The `skygate-core` library contains math and geometry functions and
  constants, physical constants, time and date conversion functions and
  constants.
- Before adding a new helper function, constant, or magic number, search the
  codebase first.
- If the needed functionality already exists, use it instead of duplicating it.
- If the needed functionality does not exist, decide whether it is
  generalizable.
- If the needed functionality is generalizable, extend the existing core
  classes or structs instead of adding isolated one-off logic.

## Interfaces and Abstractions

- Prefer abstract interfaces for replaceable subsystems.
- Interface classes should:
  - Have virtual destructors.
  - Declare only virtual methods.
  - Keep destructors and all methods inline or pure virtual in the header.
  - Avoid data members.
  - Be focused on a single responsibility.
- Do not add abstraction layers without a clear need.
- Preserve existing dependency direction and responsibility boundaries.

## Error Handling and Logging

- Validate external inputs.
- Fail fast on programmer errors in debug builds.
- Return explicit error states where recovery is possible.
- When code can fail, for example because inputs are not found or a download is
  cancelled, use existing logging capabilities to report the problem.
- Do not write code that stays silent when a problem happens.
- If adding diagnostics, use consistent Qt logging categories.
- Do not assume a pre-existing project-wide logging category scheme where none
  exists yet.

## Testing

- Add or adjust Qt Test coverage for new logic, especially math/transform code.
- Keep rendering-independent calculations testable without UI.
- Prefer deterministic tests with fixed time/location inputs.
- Register tests through the existing per-module CMake helpers and CTest flow.
- Do not weaken, delete, or skip tests to make the build pass.

## Formatting

- After changing any C++ source or header, run `clang-format` on the touched
  C++ files before considering the change complete.
- When editing Markdown files, `*.md`, keep line width at or below 80
  characters unless a long path, URL, symbol, or name does not fit.

## Agent Workflow

- Make minimal, focused changes.
- Do not rewrite unrelated files.
- Do not reformat unrelated files.
- Prefer existing presets and build trees over inventing new local workflow
  instructions.
- Update documentation when behavior, contracts, or architecture meaningfully
  change.
- After making required changes, always compile the project and make sure tests
  are green.
- If a rule conflicts with an explicit user instruction, follow the user
  instruction.
