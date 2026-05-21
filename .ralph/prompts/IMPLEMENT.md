You are autonomous software agent implementing tasks. Here are the rules:

1. Study `specs/<spec-file>.md` to learn about the feature
   specification you are working on.
2. Study code of the project in `libs/*` and `apps/*`, use up to 5 subagents.
3. Study `IMPLEMENTATION_PLAN.md`.
4. Your goal is to choose ONE (and only ONE) task from `IMPLEMENTATION_PLAN.md`
   based on following scoring and criteria:
   - task MUST not be marked as done;
   - higher priority is better;
   - task MUST not have unfinished dependent tasks;
   - task MUST not contain FAILED status in Notes section.
   Do NOT mark task as done, it would be done at later stage.
5. Write (do NOT append, always overwrite) into file `.ralph/active_task.md`
   the ID of your selected task and nothing else.
6. Before making any change search codebase, don't assume something not
   implemented. You can use up to 5 subagents.
7. Implement your task, pay attention to required work and verifications you
   was asked to perform. Run tests and make sure they all pass. If any
   functionality is missing then it's your job to add it to application
   specifications. Think hard. Use `build-ralph` folder as out-of-source build
   location for testing. Do NOT create other build directories.
8. When you discover another issue NOT related to your task, immediately update
   `IMPLEMENTATION_PLAN.md` by adding another task with your findings using a
   subagent. Do NOT implement another task, leave it for another iteration.
9. When you discover another issue RELATED to your task, immediately update
   `IMPLEMENTATION_PLAN.md` for current task, and fix the issue.
10. If you have some really important findings, like unrelated issues you
    fixed, put it into Notes section of the task description.
11. Write the final status of the task and implementation details in file
    (create if not exists) `.ralph/<spec-name>/<task-id>/implementation.md`
    using the following exemplary template:
   ```
    ## Task
    - ID: <task-id>
    - Title: <task-title>

    ## Status
    READY | FAILED

    ## Acceptance criteria claimed
    - [x] Public result status enum added
    - [x] Request/result types exposed in public API
    - [x] Tests added for basic construction/status behavior
    - [x] Existing tests pass

    ## Files changed
    - `include/ephemeris/api.hpp`
    - `src/ephemeris/api.cpp`
    - `tests/ephemeris_result_status_test.cpp`

    ## Important notes
    - Additional issue resolved in `src/libs/common/utilities.cpp`
   ```
12. Commit changes to git in a single commit, use prefix `[<task-ID>][impl]` in
    message. Do not commit changes you did not implement, i.e. unstaged files.
    Do not commit `IMPLEMENTATION_PLAN.md` and `active_task.md`.
13. If for some reason you FAILED to implement the task:
    - Immediately update Notes section of the task in `IMPLEMENTATION_PLAN.md`
      to FAILED.
    - Do NOT mark task as done in `IMPLEMENTATION_NOTES.md`, leave it as not
      done.
    - Leave your last changes in code as it is and output
      <promise>FAILED</promise>.
