# minitrace.h - A simple and minimal stacktrace built on top of libunwind and libdw

minitrace.h is a stack trace library which allows you to retrieve the current stack trace and print it.
The library depends on libunwind and libdw.

## Basic example
```c
// Basic example of the library that uses the function 'mt_print_current_stack_trace' 
// to prints the current stack trace
#define MT_IMPLEMENTATION
#include "minitrace.h"

__attribute__((always_inline))
static inline void deeply_inlined_callee(void) {
    mt_print_current_stack_trace();
}

__attribute__((always_inline)) 
static inline void outer_inlined_caller(void) {
    deeply_inlined_callee();
}

void standard_frame_b(void) {
    outer_inlined_caller();
}

void standard_frame_a(void) {
    standard_frame_b();
}

int main(void) {
    standard_frame_();
    return 0;
}
```

## How to use the library in your own project

> [!WARNING]
> Remember to compile with debug information!

The only file you need from here is [minitrace.h](https://github.com/Nickys0/minitrace.h/blob/main/minitrace.h). Just copy-paste it to your project and start using it.
Aftery copy-paste include "minitrace.h" in your project and define MT_IMPLEMENTATION to include the implementation.

When compiling ensure you link with libunwind and libdw:

```bash
cc -Wall -Wextra -g $(pkg-config --cflags libdw libunwind) -o <your_exe> <you_files...> $(pkg-config --libs libdw libunwind)
```

## Configuration

The library lets you define some MACROS before including the library to change its behaviour:

You can define this to use different allocators/assertions which are compatible with the libc related functions:

```c
#define MS_ASSERT(expr) // (default to libc's assert macro)
#define MS_MALLOC(nbytes) // (default to libc's malloc function)
#define MS_REALLOC(old_ptr, new_nbytes) // (default to libc's realloc function)
#define MS_FREE(ptr) // (default to libc's free function)
```

## How to run tests

To run tests under ./tests just compile run_tests like this:

```bash
gcc -o tester run_tests.c -Wall -Wextra
```

and then run it:

```bash
./tester
```

## Compatibility 
The library only knows the DWARF format so any other format (like the Windows PDB) won't work.

The library is tested on Linux.

On Windows using GCC under environments like Cygwin or MSYS2 can produce PE (Portable Executable) files containing DWARF debug info. but IS NOT TESTED