# PHL - Embeddable PHP Engine

A lightweight, portable, embeddable implementation of PHP written in C.

## About

PH7 is an in-process software library that implements a highly-efficient embeddable bytecode compiler and virtual machine for the PHP programming language. It allows host applications to compile and execute PHP scripts in-process, making PH7 to PHP what SQLite is to SQL.

This is a refactored version of the original PH7 repository, revived after years of dormancy with modern development practices and a focus on reliability.

## Project Goals

- **Lightweight**: Keep the engine suitable for embedded systems and resource-constrained environments
- **Portable**: Maintain compatibility across platforms with minimal dependencies
- **Reliable**: Introduce extensive testing and state-of-the-art continuous integration
- **Compatible**: Refactor PH7's original idiosyncrasies to be more compatible with official PHP.

## PH7 (Original) Features

- **PHP 5.3 Constructs**: Support for heredoc, nowdoc, goto, classes, anonymous functions, closures, and more
- **Extended Language Features**:
  - Function & Method Overloading
  - Full Type Hinting
  - Comma expressions
  - String comparison operators (`eq` and `ne`)
  - Improved operator precedence
  - Powerful object-oriented subsystem
  - Complex expressions as default function argument values
  - 64-bit integer arithmetic on all platforms
  - Native UTF-8 support

- **Embeddable Architecture**:
  - Written in ANSI C
  - Thread-safe and fully reentrant
  - Multiple interpreter states can coexist without interference
  - Single-file amalgamation build available

- **Rich Built-in Library** (470+ functions):
  - XML parser with namespace support
  - INI processor
  - CSV reader/writer
  - UTF-8 encoder/decoder
  - ZIP archive extractor
  - JSON encoder/decoder
  - Random number/string generator
  - Native File I/O for Windows and UNIX

## Quick Start

### Building

```bash
make
```

The default build produces executables in `build/x86_64-linux-gnu/` (or your platform's equivalent).

### Hello World Example

```c
#include "src/ph7/ph7.h"
#include <stdio.h>

static int output_consumer(const void *pOutput, unsigned int nLen, void *pUserData) {
    printf("%.*s", nLen, (const char *)pOutput);
    return PH7_OK;
}

int main(void) {
    ph7 *pEngine;
    ph7_vm *pVm;
    const char *code = "<?php echo 'Hello, World!'; ?>";
    
    // Initialize engine
    ph7_init(&pEngine);
    
    // Compile PHP code
    ph7_compile_v2(pEngine, code, -1, &pVm, 0);
    
    // Configure output handler
    ph7_vm_config(pVm, PH7_VM_CONFIG_OUTPUT, output_consumer, 0);
    
    // Execute
    ph7_vm_exec(pVm, 0);
    
    // Cleanup
    ph7_vm_release(pVm);
    ph7_release(pEngine);
    
    return 0;
}
```

Compile with:
```bash
gcc -o hello hello.c src/ph7/ph7amalgam.c
```

### Using the Interpreter

The project includes a standalone PHP interpreter:

```bash
# Run a PHP file
./build/x86_64-linux-gnu/phl examples/hello_world.php

# With script arguments
./build/x86_64-linux-gnu/phl script.php arg1 arg2
```

## Examples

See the [`examples/`](examples/) directory for more usage examples:

- `ph7_intro.c` - Basic embedding example
- `ph7_cgi.c` - CGI-style execution
- `ph7_func_intro.c` - Registering C functions
- `ph7_const_intro.c` - Defining constants

## Testing

*(Testing infrastructure is being developed as part of this refactor)*

```bash
# Run tests
make test
```

## Documentation

- [Original API Documentation](http://ph7.symisc.net/c_api.html)
- [Embedding Guide](http://ph7.symisc.net/intro.html)
- [Foreign Function Interface](http://ph7.symisc.net/func_intro.html)

## License

PH7 is licensed under the BSD 3-Clause License. See [`LICENSES/`](LICENSES/) for details.

## History

PH7 was originally developed in 2011 for embedded router web interfaces, providing a dynamic scripting environment in resource-constrained devices. This refactored version aims to modernize the codebase while preserving its lightweight, embeddable nature.

## Contributing

Contributions are welcome! This refactor focuses on:
- Improving test coverage
- Setting up modern CI/CD pipelines
- Maintaining backward compatibility
- Keeping the codebase portable and lightweight

## Differences from Original

This refactored version maintains API compatibility with the original PH7 while introducing:
- Modern build system improvements
- Comprehensive test suite
- Continuous integration
- Better documentation
- Active maintenance

---

**Note**: For the historical README, see [`OLDREADME.md`](OLDREADME.md).
