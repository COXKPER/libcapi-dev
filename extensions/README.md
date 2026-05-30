# socket.so Extension

The `socket.so` extension provides native TCP socket support for `libcapi`.
This extension relies on `libcapi`'s native plugin architecture and uses `sys/socket.h` to perform networking requests.

## Installation

When `libcapi` is built and installed (`make install`), the `socket.so` shared library is compiled and automatically placed in `/usr/capi/libs/socket.so`.

## Usage

To use the socket extension, you can import it into your script using the newly supported `<>` syntax, which maps directly to the `/usr/capi/libs/` directory.

```capi
import <socket.so>;
```

### socket.get

The extension registers the `socket.get` command, which allows you to perform an HTTP GET request to port 80 of a given hostname.

```capi
# Perform a GET request to google.com
socket.get "google.com";

# The result is automatically stored in the SOCKET_RESPONSE variable
echo: SOCKET_RESPONSE;
```
