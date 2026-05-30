package main

/*
#cgo CFLAGS: -I../include
#cgo LDFLAGS: -L.. -lcapi
#include <stdlib.h>
#include "libcapi.h"
*/
import "C"

import (
	"fmt"
	"os"
	"unsafe"
)

func main() {
	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	command := os.Args[1]

	switch command {
	case "version":
		fmt.Printf("cacli (Go Wrapper for libcapi)\n%s\n", C.GoString(C.capi_version()))
	case "info":
		fmt.Print(C.GoString(C.capi_info()))
	case "run":
		if len(os.Args) < 3 {
			fmt.Println("Error: missing filename for run command.")
			fmt.Println("Usage: cacli run <file.capi>")
			os.Exit(1)
		}
		filename := os.Args[2]
		runScript(filename)
	case "help":
		printUsage()
	default:
		fmt.Printf("Unknown command: %s\n", command)
		printUsage()
		os.Exit(1)
	}
}

func printUsage() {
	fmt.Println("cacli - Create Application Programmable Interface CLI")
	fmt.Println("Usage: cacli <command> [arguments]")
	fmt.Println("\nCommands:")
	fmt.Println("  run <file>   Execute a .capi script")
	fmt.Println("  version      Print libcapi version")
	fmt.Println("  info         Print libcapi build and feature info")
	fmt.Println("  help         Print this help message")
}

func runScript(filename string) {
	cFilename := C.CString(filename)
	defer C.free(unsafe.Pointer(cFilename))

	C.capi_reset()
	res := C.capi_process_file(cFilename)

	if res.error_code != 0 {
		fmt.Fprintf(os.Stderr, "[CAPI ERROR] %s\n", C.GoString(&res.error_msg[0]))
		os.Exit(int(res.error_code))
	} else {
		output := C.GoString(&res.output[0])
		if output != "" {
			fmt.Print(output)
		}
	}
}
