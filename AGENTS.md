# AGENTS.md

## Purpose
This file defines coding and collaboration rules for agents working in this
repository.

## C++ Conventions
- Use UTF-8 and prefer ASCII in source unless non-ASCII is required.
- Prefer `.hpp` headers and `#pragma once`.
- Prefer one class per header/source pair where practical.
- Keep functions short and focused.
- Use `const` correctness consistently.
- Use `override` for overridden virtual functions.
- Prefer `nullptr` over `0`/`NULL`.
- Prefer `[[nodiscard]]` on query/factory APIs when ignoring the result is
  likely a bug.
- Namespace-level factory/helper APIs in public headers are acceptable when
  they are part of the intended module interface. Avoid ad-hoc public free
  functions that do not define a clear API boundary.
- Prefer unnamed-namespace helpers for `.cpp`-local and test-local functions.
- Keep public library headers under `libs/*/include/skygate/...`. Keep
  internal-only helpers in `src/` where practical.
- Types/classes: `PascalCase`.
- Interface classes: prefix with `I` (example: `IRenderer`, `IProjection`).
- Methods/functions: `camelCase`.
- Member variables: `m_camelCase`.
- Local variables/parameters: `camelCase`.
- Constants/macros: `kPascalCase` for constants, avoid macros unless necessary.
- Setters are prefixed with `set` (example: `setLatitude(double latitude)`).
- Getters use the property name directly (example: `latitude() const`).
- Boolean getters may use `is`/`has` when clearer (example: `isPaused() const`).

## Interfaces and Abstractions
- Prefer abstract interfaces for replaceable subsystems.
- Interface classes should:
  - Have virtual destructors.
  - Avoid data members.
  - Be focused on a single responsibility.

## Qt/CMake Expectations
- Target Qt 6.5+ and C++20.
- Use CMake for all build configuration.
- Prefer the existing CMake presets and build targets when possible:
  `core-debug`, `ui-debug`, and `ui-run`.
- Preserve the one-way dependency direction:
  `skygate-ui -> skygate-ephemeris -> skygate-core`.
- Keep platform-specific code isolated behind interfaces.
- Favor Qt abstractions when they preserve portability.
- For QML-facing types, follow the established `QObject`/`Q_PROPERTY`/
  `Q_INVOKABLE` pattern instead of inventing alternate UI binding layers.

## Error Handling and Logging
- Validate external inputs.
- Fail fast on programmer errors in debug builds.
- Return explicit error states where recovery is possible.
- If adding diagnostics, use consistent Qt logging categories. Do not assume a
  pre-existing project-wide category scheme where none exists yet.

## Testing
- Add or adjust Qt Test coverage for new logic, especially math/transform code.
- Keep rendering-independent calculations testable without UI.
- Prefer deterministic tests (fixed time/location inputs).
- Register tests through the existing per-module CMake helpers / CTest flow.

## Agent Workflow
- Make minimal, focused changes.
- Do not rewrite unrelated files.
- After changing any C++ source or header, run `clang-format` on the touched
  C++ files before considering the change complete.
- Prefer existing presets and build trees over inventing new local workflow
  instructions.
- Update documentation when behavior, contracts, or architecture meaningfully
  change.
- If a rule conflicts with explicit user instruction, follow the user
  instruction.
