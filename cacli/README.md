# cacli (Capi CLI)

`cacli` is a powerful command-line interface for the `libcapi` scripting language, written in Go.
It leverages Go's CGO capabilities to safely interface directly with the `libcapi.so` shared library.

## Installation

When building the main project, `cacli` is compiled automatically.
You can install it system-wide with:
```bash
sudo make install
```
This installs both `libcapi` and the `cacli` binary.

## Usage

```bash
cacli <command> [arguments]
```

### Commands

*   `run <file>`: Execute a `.capi` script file.
    *   Example: `cacli run examples/hello.capi`
*   `version`: Print the version of the underlying `libcapi` engine.
*   `info`: Print build details and supported features of the `libcapi` engine.
*   `help`: Print the help message.

## Development

`cacli` is a standard Go module located in the `cacli/` directory.

To build it manually (assuming `libcapi.so` is already compiled):
```bash
cd cacli
go build -o cacli
```

> **Note:** To run `cacli` locally without installing the library globally, ensure `LD_LIBRARY_PATH` points to the `libcapi-dev` root directory.
> ```bash
> export LD_LIBRARY_PATH=$PWD
> ./cacli/cacli version
> ```
