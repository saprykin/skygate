## Planning (pass 1)

Study `spec/high-precision-ephemeris-engine.md`, `libs/*`, `apps/*`, and
`IMPLEMENTATION_PLAN.md` to understand specs, sources and plan so far. Use up
to 5 subagents in parallel to study specs and sources. Do not implement code
changes.

Every task in `IMPLEMENTATION_PLAN.md` must use this format:

```
### FP-NNN: Short title

Status: [ ] | [x]
Priority: Critical | High | Medium | Low
Type:
Source:
- Spec:
Dependencies:
Problem:
Required work:
Verification:
Notes:
```

Do not delete completed tasks.
Mark a task [x] only if the code exists and verification passes.

First task is to study `IMPLEMENTATION_PLAN.md` (it may be incorrect) and is to
use up to 5 subagents to study existing source code in `libs/*` and `apps/*`,
and compare it against the specifications. From that create/update
`IMPLEMENTATION_PLAN.md` according to presented format, sorted in priority of
the items which have yet to be implemented. Think extra hard and use the
oracle to plan. Study `IMPLEMENTATION_PLAN.md` to determine starting point for
research and keep it up to date with items considered complete/incomplete using
subagents.

Your goal is to refine the plan from `IMPLEMENTATION_PLAN.md` and output it
back into `IMPLEMENTATION_PLAN.md`.

Rules:
- Do not implement code.
- Do not invent requirements not present in the spec.
- Split tasks that are too broad.
- Keep algorithmic/numerical implementation separate from public API/modelling
  tasks.
- Keep data ingestion/loading separate from computation and validation.

Each refined task must be clear and actionable. It must be clear what exactly
to do, how to verify if task implemented correctly. Remove ambiguity.

Output only the updated `IMPLEMENTATION_PLAN.md`.

## Planning (pass 2)

Study `spec/high-precision-ephemeris-engine.md`, the current source tree and
current implementation plan `IMPLEMENTATION_PLAN.md`. Use up to 5 agents to
study these files.

Update `IMPLEMENTATION_PLAN.md` by preserving the existing phase structure,
but split only the following oversized items into smaller, clear and actionable
subtasks:

- HP-006
- HP-008
- HP-022
- HP-027
- HP-041

Also review HP-014, HP-026, and HP-032 and split them only if they still
combine independent implementation concerns.

Rules:
- Do not split other tasks.
- Do not renumber unrelated tasks unnecessarily unless needed.
- Preserve dependencies across tasks.
- Keep the original parent task as a short heading or umbrella item if useful.
- Do not implement code.

Output only the updated `IMPLEMENTATION_PLAN.md`.
