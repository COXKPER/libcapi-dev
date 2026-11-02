# AGENT.md

## Project

`libcapi` is a small C library and scripting runtime. Keep it easy to embed, easy to read, and dependency-light. The current release is `2.5.0`.

## Non-breaking change policy

Do not break existing users, scripts, plugins, or build commands.

- Preserve existing C symbols and signatures.
- Preserve existing `.capi` syntax and behavior.
- Add syntax as aliases or extensions. Do not repurpose old syntax.
- Do not remove `int`, `echo:`, `incl`, `import`, or existing built-ins.
- Keep old plugin handlers source-compatible.
- Update the version only when the public behavior justifies it.
- If a breaking change is unavoidable, introduce a new API or syntax, document migration, add regression tests, and clearly mark the old path as deprecated before removal.
- Never silently change output, error semantics, path rules, or variable behavior.

## Implementation rules

- Prefer the smallest working change. Avoid framework code and new dependencies.
- Keep the core usable with the C standard library and existing POSIX requirements.
- Validate inputs at public API boundaries.
- Preserve fixed buffer limits unless a change is explicitly designed and tested.
- Return clear errors. Do not hide failures or turn invalid input into valid-looking data.
- Keep plugins optional. New integrations belong in extensions, not in the interpreter core.
- Do not add sandboxing, parental controls, GUI code, or unrelated product features unless explicitly requested.
- Do not commit generated binaries or build artefacts unless the repository already tracks the specific artefact.

## Public C API

Include the installed header:

```c
#include <capi/libcapi.h>
```

Core execution:

```c
CapiResult capi_process_file(const char *filepath);
void capi_process_line(const char *line, CapiResult *res);
void capi_reset(void);
```

`CapiResult` contains `output`, `force_close`, `error_code`, and `error_msg`. Always initialize or use the returned result as appropriate. Check `error_code` after processing a file.

Variables and metadata:

```c
void capi_set_variable(const char *name, const char *value);
const char *capi_get_variable(const char *name);
const char *capi_version(void);
const char *capi_info(void);
```

Plugin API:

```c
typedef int (*CapiCommandHandler)(const char *line, CapiResult *res);
int capi_register_command(const char *prefix, CapiCommandHandler handler);
```

A plugin should export `capi_init`:

```c
#include <capi/libcapi.h>
#include <string.h>

static int hello_handler(const char *line, CapiResult *res)
{
    if (strcmp(line, "hello.world;") != 0) return 0;
    size_t used = strlen(res->output);
    if (used < sizeof(res->output))
        snprintf(res->output + used, sizeof(res->output) - used, "Hello!\n");
    return 1;
}

void capi_init(void)
{
    capi_register_command("hello.", hello_handler);
}
```

Use the public API only. `include/internal.h` is not an extension API and must not be installed or depended on by plugins. Build a plugin as a shared object and load it from CAPI with:

```capi
import "./hello.so";
hello.world;
```

A handler returns nonzero only when it consumed the line. Prefixes should be specific. Check the return value of `capi_register_command`: `0` means success and `-1` means invalid input or a full command table.

## Language compatibility

Existing syntax remains valid:

```capi
int name = "Capi"
echo: name;
```

Friendly aliases are also valid:

```capi
let name = "Capi"
var age = "5"
print name;
say "Halo!";
```

## Required validation

Before declaring a change complete:

```bash
make test
git diff --check
```

The regression suite must pass. Add a focused test for every non-trivial language, API, or plugin change. Review the diff for accidental compatibility changes.
