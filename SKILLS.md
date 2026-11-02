# SKILLS.md

## Purpose

Install this file as project guidance for an AI coding agent working on `libcapi`.

## Skill: maintain-capi

Use when modifying the C library, interpreter, public headers, tests, or build files.

### Instructions

1. Read `AGENT.md`, `README.md`, `include/libcapi.h`, and the relevant source before editing.
2. Preserve backward compatibility. Existing C symbols, plugin signatures, `.capi` syntax, built-ins, and build commands are contracts.
3. Prefer a small standard-library implementation. Do not add dependencies or unrelated abstractions.
4. Add new language behavior as an alias or additive syntax. Never silently reinterpret old syntax.
5. Keep plugin integrations optional and place them in `extensions/` when they are not core functionality.
6. Add a focused regression test for every non-trivial change.
7. Run `make test` and `git diff --check` before reporting completion.
8. Report changed files, compatibility impact, and exact validation results.

## Skill: extend-capi-plugin

Use when creating or changing a CAPI library/plugin.

### Instructions

1. Include only the installed public header:

   ```c
   #include <capi/libcapi.h>
   ```

2. Export:

   ```c
   void capi_init(void);
   ```

3. Register a specific command prefix:

   ```c
   int rc = capi_register_command("example.", handler);
   ```

4. Return nonzero from the handler only when the line was consumed.
5. Append output within `CapiResult.output` without exceeding its capacity.
6. Build as a shared object with `-shared -fPIC` and load it using:

   ```capi
   import "./example.so";
   ```

7. Check the registration return code. `0` is success; `-1` means invalid input or a full command table.
8. Add a runtime or registration test and document the script commands exposed by the plugin.

## Skill: review-capi-change

Use before merging or releasing a change.

### Checklist

- [ ] No existing public symbol or syntax was removed or changed incompatibly.
- [ ] New behavior has a regression test.
- [ ] Errors are explicit and do not silently become successful values.
- [ ] Buffer writes are bounded.
- [ ] No generated binaries or temporary files are included accidentally.
- [ ] `make test` passes.
- [ ] `git diff --check` passes.
- [ ] README or API documentation matches the implementation.

## Installation

For agents that support project instruction files, place `AGENT.md` and `SKILLS.md` at the repository root. For a global agent setup, copy the files into that agent's project-rules or skills directory, then ensure the agent loads `AGENT.md` before making changes and exposes the three skills above as project skills.
