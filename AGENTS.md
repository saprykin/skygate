# AGENTS.md

## Purpose
This file defines coding and collaboration rules for agents working in this
repository.

## General Coding Style (C++)
- Use 4-space indentation.
- Use UTF-8 and prefer ASCII in source unless non-ASCII is required.
- Prefer one class per header/source pair where practical.
- Keep functions short and focused.
- Use `const` correctness consistently.
- Use `override` for overridden virtual functions.
- Prefer `nullptr` over `0`/`NULL`.

## Naming Conventions
- Types/classes: `PascalCase`.
- Interface classes: prefix with `I` (example: `IRenderer`, `IProjection`).
- Methods/functions: `camelCase`.
- Member variables: `m_camelCase`.
- Local variables/parameters: `camelCase`.
- Constants/macros: `kPascalCase` for constants, avoid macros unless necessary.

## Property Access Pattern
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
- Target Qt 6.
- Use CMake for all build configuration.
- Keep platform-specific code isolated behind interfaces.
- Favor Qt abstractions when they preserve portability.

## Error Handling and Logging
- Validate external inputs.
- Fail fast on programmer errors in debug builds.
- Return explicit error states where recovery is possible.
- Use consistent logging categories for diagnostics.

## Testing
- Add unit tests for core logic, especially math/transform code.
- Keep rendering-independent calculations testable without UI.
- Prefer deterministic tests (fixed time/location inputs).

## Agent Workflow
- Make minimal, focused changes.
- Do not rewrite unrelated files.
- Update documentation when behavior/contracts change.
- If a rule conflicts with explicit user instruction, follow the user
  instruction.
