# 🔧 libcapi — Create Application Programmable Interface

![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)
![License](https://img.shields.io/badge/license-GPL--3.0-green.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)

## Description

**libcapi** is a lightweight C library that provides a custom scripting language for creating application programmable interfaces. CAPI scripts (`.capi` files) support variables, functions, conditionals, loops, math operations, string manipulation, arrays, shell command execution, file I/O, environment variable access, and calibration file support (`.calib`).

Designed for embedding into C/C++ applications, libcapi gives your programs a simple yet powerful scripting layer with zero external dependencies beyond the C standard library and `libdl`.

---

## Features

### Core
- 📝 **Custom scripting language** with simple, readable syntax
- 📂 **File-based processing** — load and execute `.capi` script files
- 🔗 **Line-by-line processing** — feed individual lines to the interpreter
- 📦 **Include system** — include other `.capi` files with `incl`
- 🐚 **Shell execution** — run system commands from scripts

### Control Flow
- 🔀 **Conditionals** — `if`/`else`/`endif` branching
- 🔁 **Loops** — `while`/`endwhile` loops
- 🛡️ **Error handling** — `try`/`catch`/`endtry` blocks

### Data
- 📊 **Variables** — string and numeric variable storage
- ➕ **Math operations** — arithmetic on variables
- 🔤 **String operations** — length, upper, lower, concat
- 📋 **Arrays** — create, push, get, and measure arrays

### Built-ins
- ⏱️ **Timer** — start/stop execution timing
- 😴 **Sleep** — pause execution for milliseconds
- 🔍 **Version & Info** — query library metadata
- 🌐 **Environment** — get and set environment variables
- 📁 **File I/O** — read, write, and append to files

### Configuration
- ⚙️ **Calib files** — INI-style configuration parser (`.calib`)
- 📖 **Typed access** — string, integer, float, and boolean getters
- 💾 **Save/load** — read and write configuration files programmatically

---

## Quick Start

```bash
# Clone the repository
git clone https://github.com/neoncorp/libcapi.git
cd libcapi

# Build the library
make

# Run the examples
# (After building, link your program against libcapi.so or libcapi.a)
```

### Hello World

Create a file called `hello.capi`:

```
# Hello World in CAPI
int msg= "Hello, World!"
echo: msg;
```

Process it from C:

```c
#include <capi/libcapi.h>
#include <stdio.h>

int main(void) {
    CapiResult res = capi_process_file("hello.capi");
    printf("%s", res.output);
    return 0;
}
```

Compile and run:

```bash
gcc -o hello hello.c -lcapi -ldl
./hello
```

---

## Language Reference

### Comments

Lines starting with `#` or `//` are comments and are ignored.

```
# This is a comment
// This is also a comment
```

### Variables

Declare variables with `int name= "value"`:

```
int greeting= "Hello World"
int count= "42"
int flag= "true"
```

> **Note:** All values are stored as strings internally. The `int` keyword is used for all variable declarations regardless of type.

### Echo (Output)

Print variable values or literal text:

```
int name= "Alice"
echo: name;
echo: Hello there!;
```

### Functions

Define reusable code blocks:

```
function greet:
    echo: Welcome!;
    echo: Have a great day!;
funclose;

# Call the function
greet();
```

### 3. File Inclusion & Imports

You can include other `.capi` scripts or load `.so` shared libraries. The `incl` command executes the file (or loads the library) every time it is called. The `import` command executes the file (or loads the library) only once per script execution, preventing circular dependencies and double-execution.

```capi
# Include executes every time
incl <path_to_file.capi>;

# Import executes only once
import "path_to_file.capi";

# Load a shared library (must end in .so)
import "socket.so";
incl socket.so;
```

For shared libraries, the interpreter will use `dlopen` to load the library and attempt to call a `capi_init` function if it exists:

```c
void capi_init(void) {
    // Plugin initialization code
}
```

### Shell Execution

Execute shell commands:

```
capi.exec: "ls -la"
capi.exec: "echo Hello from shell"
```

### Conditionals

Branch with `if`/`else`/`endif`:

```
int mode= "debug"

if mode == "debug":
    echo: Debug mode enabled;
else:
    echo: Production mode;
endif;
```

### Loops

Repeat with `while`/`endwhile`:

```
int i= "0"
while i < "5":
    echo: i;
    math i = i + 1;
endwhile;
```

### Math Operations

Perform arithmetic:

```
int a= "10"
int b= "3"

math sum = a + b;
math diff = a - b;
math prod = a * b;
math quot = a / b;

echo: sum;
echo: diff;
echo: prod;
echo: quot;
```

### String Operations

Manipulate strings:

```
int text= "hello world"

# Get string length
str.len text;

# Convert to uppercase
str.upper text;
echo: text;

# Convert to lowercase
str.lower text;
echo: text;

# Concatenate strings
int a= "Hello"
int b= " World"
str.concat a b;
echo: a;
```

### Arrays

Work with arrays:

```
# Create an array
array colors = "red" "green" "blue";

# Get array length
array.len colors;

# Access by index (0-based)
array.get colors 0;

# Append an element
array.push colors "yellow";

# Check new length
array.len colors;
```

### Timer

Measure execution time:

```
capi.timer start;
# ... code to time ...
capi.timer stop;
```

### Sleep

Pause execution (in milliseconds):

```
capi.sleep 1000;
```

### Version & Info

Query library metadata:

```
capi.version;
capi.info;
```

### Environment Variables

Access and modify environment variables:

```
# Get an environment variable
env.get HOME;
env.get PATH;

# Set an environment variable
env.set MY_VAR "my_value";
```

### File I/O

Read, write, and append to files:

```
# Read a file
file.read "/tmp/input.txt";

# Write to a file (overwrites)
file.write "/tmp/output.txt" "Hello, File!";

# Append to a file
file.append "/tmp/output.txt" "Another line";
```

### Try/Catch

Handle errors gracefully:

```
try:
    # Code that might fail
    capi.exec: "some_risky_command"
catch:
    echo: An error occurred!;
endtry;
```

### Calib File Operations

Load and query `.calib` configuration files from scripts:

```
# Load a calibration file
calib.load "config.calib";

# Get a value (section.key)
calib.get settings.timeout;
```

### Force Close (Deprecated)

Force the interpreter to stop:

```
capi.forceclose();
```

> ⚠️ **Deprecated:** This function is deprecated and may be removed in a future version. Use proper control flow instead.

---

## .calib File Format

Calib files use an INI-style format with sections, keys, and typed values.

### Structure

```ini
# Comment lines start with #

[section_name]
key = value
another_key = "quoted string value"
numeric_key = 42
float_key = 3.14
bool_key = true
```

### Supported Types

| Type | Examples | Notes |
|------|----------|-------|
| **String** | `"hello"`, `hello` | Quotes optional for simple values |
| **Integer** | `42`, `-7`, `0` | Parsed with `atoi()` |
| **Float** | `3.14`, `-0.5` | Parsed with `atof()` |
| **Boolean** | `true`, `false`, `yes`, `no`, `1`, `0`, `on`, `off` | Case-insensitive |

### Example

```ini
# Application Configuration
[app]
name = "MyApp"
version = "1.0"
debug = false

[server]
host = "localhost"
port = 8080
max_workers = 4

[database]
driver = "sqlite"
path = "/var/lib/myapp/data.db"
```

### C API Usage

```c
#include <capi/calib.h>

CalibFile cf;
calib_load(&cf, "config.calib");

const char *name = calib_get_string(&cf, "app", "name");
int port         = calib_get_int(&cf, "server", "port", 3000);
double timeout   = calib_get_float(&cf, "server", "timeout", 5.0);
int debug        = calib_get_bool(&cf, "app", "debug", 0);

calib_free(&cf);
```

---

## C API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `CapiResult capi_process_file(const char *filepath)` | Process a `.capi` script file and return accumulated output |
| `void capi_process_line(const char *line, CapiResult *res)` | Process a single line of CAPI script |
| `void capi_reset(void)` | Reset all interpreter state (variables, functions, arrays) |
| `const char *capi_version(void)` | Return the library version string |
| `const char *capi_info(void)` | Return build/feature information |

### Error Reporting

| Function | Description |
|----------|-------------|
| `void capi_report_error(const char *msg, int line, const char *file)` | Report a script error with location |
| `void capi_report_warning(const char *msg)` | Report a non-fatal warning |

### Calib Functions

| Function | Description |
|----------|-------------|
| `int calib_load(CalibFile *cf, const char *filepath)` | Load and parse a `.calib` file (returns 0 on success) |
| `void calib_free(CalibFile *cf)` | Free a CalibFile structure |
| `const char *calib_get_string(const CalibFile *cf, const char *section, const char *key)` | Get a config value as a string |
| `int calib_get_int(const CalibFile *cf, const char *section, const char *key, int default_val)` | Get a config value as an integer |
| `double calib_get_float(const CalibFile *cf, const char *section, const char *key, double default_val)` | Get a config value as a float |
| `int calib_get_bool(const CalibFile *cf, const char *section, const char *key, int default_val)` | Get a config value as a boolean |
| `int calib_set(CalibFile *cf, const char *section, const char *key, const char *value)` | Set a key-value pair in a section |
| `int calib_save(const CalibFile *cf, const char *filepath)` | Write the CalibFile back to disk |

### Result Structure

```c
typedef struct {
    char   output[CAPI_MAX_OUTPUT]; /* Accumulated script output       */
    int    force_close;             /* Set if capi.forceclose() called */
    int    error_code;              /* 0 = success, nonzero = error    */
    char   error_msg[256];          /* Human-readable error message    */
} CapiResult;
```

---

## Build & Install

### Build

```bash
make            # Build libcapi.so and libcapi.a
make test       # Build and run the test suite
make clean      # Remove build artifacts
```

### Install

```bash
sudo make install     # Install to /usr/local/lib and /usr/local/include/capi
sudo make uninstall   # Remove installed files
```

### Linking

```bash
# Dynamic linking
gcc -o myapp myapp.c -lcapi -ldl

# Static linking
gcc -o myapp myapp.c -L/path/to/lib -Wl,-Bstatic -lcapi -Wl,-Bdynamic -ldl
```

---

## PPA Installation

For Ubuntu/Debian systems, libcapi is available via PPA:

```bash
sudo add-apt-repository ppa:neoncorp/libcapi
sudo apt update
sudo apt install libcapi
```

---

## Project Structure

```
libcapi/
├── include/
│   ├── libcapi.h          # Public API header
│   ├── calib.h            # Calib parser public header
│   └── internal.h         # Internal declarations (not installed)
├── src/
│   ├── libcapi.c          # Core interpreter
│   ├── calib.c            # Calib file parser
│   ├── features.c         # Control flow, math, strings, arrays
│   └── builtins.c         # Built-in commands (timer, env, file I/O, etc.)
├── tests/
│   ├── test_main.c        # Test suite
│   ├── test.capi          # Test CAPI script
│   └── test.calib         # Test calibration file
├── examples/
│   ├── hello.capi         # Hello world example
│   ├── functions.capi     # Functions example
│   ├── advanced.capi      # Full-featured example
│   └── config.calib       # Example configuration file
├── .github/
│   └── workflows/
│       └── makefile.yml   # CI/CD pipeline
├── Makefile               # Build system
├── README.md              # This file
├── LICENCE                # GPL-3.0 license
└── .gitignore             # Git ignore rules
```

---

## Contributing

Contributions are welcome! Here's how to get started:

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/my-feature`
3. **Commit** your changes: `git commit -am 'Add new feature'`
4. **Push** to the branch: `git push origin feature/my-feature`
5. **Submit** a pull request

### Guidelines

- Follow existing code style (K&R braces, 4-space indentation in C)
- Compile cleanly with `-Wall -Werror`
- Add tests for new features
- Update documentation as needed

---

## License

**GPL-3.0** — Copyright © 2020 Neon Corporation

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See the [LICENCE](LICENCE) file for details.
