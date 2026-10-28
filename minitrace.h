/*
    minitrace - v0.1.0 - MIT
    
    MINITRACE LIBRARY (STB-STYLE)
    
    This library allows you to get and print colorful (or not) stack trace using 
    libunwind and libdw.

    # Macro Interface

        All these macros are `#define`d by the user before including minitrace.h

    ## Flags

        Enable or disable certain aspects of minitrace.h

        - MT_IMPLEMENTATION - Enable definitions of the functions. By default only declarations are included.

    ## Redefinable Macros

            Redefine default behaviors of minitrace.h

        - #define MT_ASSERT         (default to libc)
        - #define MT_REALLOC        (default to libc)
        - #define MT_FREE           (default to libc)
        - #define MT_STRDUP strdup  (default to libc)

   --------------------------------------------------------------------------------
   LICENSE: MIT
    
   Copyright (c) 2026 @Nickys0 <https://github.com/Nickys0>
  
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
  
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
  
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
  
*/

#ifndef _MINITRACE_H
#define _MINITRACE_H

#if defined(__linux__) || defined(__gnu_linux__)
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
#endif

#include <features.h> // internal glibc header that processes _GNU_SOURCE

// verify that glibc recognized _GNU_SOURCE
#if defined(__GLIBC__)
    #if !defined(_GNU_SOURCE)
        #error "_GNU_SOURCE was not accepted or recognized"
    #endif
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <elfutils/libdwfl.h>
#include <dwarf.h>
#include <unistd.h>

#define MT_INIT_ARR_SIZE    16
#define MT_STACK_MAX_DEPTH  200

//Regular text
#define BLACK       "\x1b[0;30m"
#define RED         "\x1b[0;31m"
#define GREEN       "\x1b[0;32m"
#define YELLOW      "\x1b[0;33m"
#define BLUE        "\x1b[0;34m"
#define MAGENTA     "\x1b[0;35m"
#define CYAN        "\x1b[0;36m"
#define WHITE       "\x1b[0;37m"
#define COLOR_RESET "\x1b[0m" //Reset

#define MT_ADDRESS_COLOR            CYAN
#define MT_FUNCTION_COLOR           MAGENTA
#define MT_FILE_PATH_MODULE_COLOR   GREEN
#define MT_COL_LINE_COLOR           CYAN

// Cross-platform MT_TYPEOF (standard in C23)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define MT_TYPEOF(expr)          typeof(expr)

// C++ Compatibility (decltype)
#elif defined(__cplusplus) 
    #if __cplusplus >= 201103L
        #include <type_traits>
        #define MT_TYPEOF(expr)          decltype(expr)
    #endif

// GCC / Clang / TinyCC (__typeof__)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__) 
    #define MT_TYPEOF(expr)          __typeof__(expr)

// MSVC (typeof extension enabled in recent versions with /std:c11 or newer)
#elif defined(_MSC_VER)
    #if _MSC_VER >= 1938 /* VS 2022 17.8+ supports typeof */
        #define MT_TYPEOF(expr)      typeof(expr)
    #elif defined(__typeof__)
        #define MT_TYPEOF(expr)      __typeof__(expr)
    #endif

#else 
    #error "typeof not supported by your compiler!"
#endif

#ifndef MT_ASSERT
    #include <assert.h>
    #define MT_ASSERT assert
#endif

#if !defined(MT_REALLOC) && !defined(MT_FREE)
    #include <stdlib.h> // for realloc, free etc.
    #define MT_REALLOC  realloc
    #define MT_FREE     free
#endif

#if !defined(MT_STRDUP)
    #define MT_STRDUP strdup
#endif

#define define_dynamic_array_of(_type, _name)   \
        typedef struct _name {                  \
            _type*   items;                     \
            uint32_t len;                       \
            uint32_t cap;                       \
        } _name

/**
 * @brief mt_da_reserve macro for dynamic array
 *        stolen from nob.h (https://github.com/tsoding/nob.h/ @v3.10.0) by @tsoding
 *        (LICENSE AT THE END OF THE FILE)
 */
#define mt_da_reserve(da, expected_capacity)                                               \
    do {                                                                                   \
        if((expected_capacity) > (da)->cap) {                                              \
            if ((da)->cap == 0) {                                                          \
                (da)->cap = MT_INIT_ARR_SIZE;                                              \
            }                                                                              \
            while((expected_capacity) > (da)->cap) {                                       \
                (da)->cap *= 2;                                                            \
            }                                                                              \
            (da)->items = (MT_TYPEOF((da)->items))MT_REALLOC(                              \
                            (da)->items, (da)->cap * sizeof(*(da)->items));                \
            MT_ASSERT((da)->items != NULL);                                                \
        }                                                                                  \
    } while (0)


/**
 * @brief mt_da_append macro for dynamic array
 *        stolen from nob.h (https://github.com/tsoding/nob.h/ @v3.10.0) by @tsoding
 *        (LICENSE AT THE END OF THE FILE)
 */
#define mt_da_append(da, item)                  \
    do {                                        \
        mt_da_reserve((da), (da)->len + 1);     \
        (da)->items[(da)->len++] = (item);      \
    } while (0)

#define mt_defer(value)                         \
    do {                                        \
        result = (value);                       \
        goto defer;                             \
    } while (0)

#define mt_da_foreach(da, ...)                                  \
    for(size_t it_idx = 0; it_idx < (da)->len; ++it_idx) {      \
        const MT_TYPEOF((da)->items) it = &(da)->items[it_idx]; \
        __VA_ARGS__                                             \
    }

#define mt_da_free(da)\
    do {                                        \
        MT_FREE((da)->items);                   \
        memset((da), 0, sizeof*(da));           \
    } while (0)

typedef uintptr_t mt_raw_address;

#define MT_OK   0
#define MT_FAIL 1
typedef int32_t mt_result;

#define MT_TAG_SUBPROGRAM   DW_TAG_subprogram
#define MT_TAG_INLINE       DW_TAG_inlined_subroutine
#define MT_TAG_ENTRY_POINT  DW_TAG_entry_point
typedef int32_t mt_tag;

#define MT_INVALID_LINE (-1)
#define MT_INVALID_COL  (-1)

struct mt_trace_loc {
    const char* file_path; // may be NULL if not found

    // I don't know why but 'dwfl_lineinfo' uses integers for line 
    // and col so we just do the same to avoid casting.
    // (-1) indicates that it was not possible to get the 
    // line and col
    int32_t    line;
    int32_t    col;
};

/**
 * @brief Structure that holds informations of a single stack trace
 */
struct mt_stack_trace {
    mt_raw_address      address;    // always given for every VALID stack trace
    struct mt_trace_loc src_loc;    // resolved position of the address in file
    const char*         mod_name;   // may be NULL if not found
    const char*         func_sign;  // function signature
    mt_tag              tag;        // tags/flags given by libdwarf
};

define_dynamic_array_of(mt_raw_address,         mt_stack_raw_traces);
define_dynamic_array_of(struct mt_stack_trace,  mt_stack_traces);

typedef struct mt_trace_loc             mt_trace_loc;
typedef struct mt_stack_raw_traces      mt_stack_raw_traces;
typedef struct mt_stack_trace           mt_stack_trace;

/**
 * @brief Structure that holds options for printing functions 
 */
struct mt__stack_print_opts {
    FILE*   stream;         // where to print the stack traces
    bool    force_color;    // if true always prints colors even if isatty(*stream*) == false
    int32_t skip;           // how many stack traces to skip (see struct mt__stack_opts)
    int32_t max_depth;      // maximum number of stack traces to get (see struct mt__stack_opts)
};
#define mt__stack_print_opts_def(...)\
        (struct mt__stack_print_opts) { .stream = stderr, .force_color = false, .skip = 0,\
                                        .max_depth = MT_STACK_MAX_DEPTH, __VA_ARGS__ }


struct mt__stack_opts {
    // how many stack traces to skip (starting from the current). 
    // (by default the library skippes its own functions)
    int32_t skip;
    int32_t max_depth; // maximum number of stack traces to get
};
#define mt__stack_opts_def(...)\
        (struct mt__stack_opts) { .skip = 0, .max_depth = MT_STACK_MAX_DEPTH, __VA_ARGS__ }

/**
 * @brief fills <out> with raw stack trace (addresses). 
 *        it is reccomended to use the wrapper 'mt_unwind_stack' 
 *        as it uses default arguments
 *
 * @warning <out> MUST BE FREED WITH 'mt_da_free'
 *
 * @returns FAILS :: != MT_OK
 */
mt_result mt_unwind_stack_(mt_stack_raw_traces* out, struct mt__stack_opts opts);
#define mt_unwind_stack(traces, ...) /* wrapper */ \
            mt_unwind_stack_((traces), mt__stack_opts_def(__VA_ARGS__))


/**
 * @brief fills <out> with resolved stack trace informations.
 *        it is reccomended to use the wrapper 'mt_get_current_stack_trace'
 *        as it uses default arguments
 *
 * @warning <OUT> MUST BE FREED with 'mt_free_stack_traces'.
 *
 * @returns FAILS :: != MT_OK
 */
mt_result mt_get_current_stack_trace_(mt_stack_traces* out, struct mt__stack_opts opts);
#define mt_get_current_stack_trace(traces, ...) /* wrapper */\
        mt_get_current_stack_trace_((traces), mt__stack_opts_def(__VA_ARGS__))


/**
 * @brief gets the current stack trace and prints it. 
 *        it is reccomended to use the wrapper 'mt_print_current_stack_trace' 
 *        as it uses default arguments
 * @returns FAILS :: != MT_OK
 */
mt_result mt_print_current_stack_trace_(struct mt__stack_print_opts opts);
#define mt_print_current_stack_trace(...) /* wrapper */ \
        mt_print_current_stack_trace_(mt__stack_print_opts_def(__VA_ARGS__))

void mt_free_stack_traces(mt_stack_traces* traces);


#if 0
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

#endif // 0

#endif // _MINITRACE_H

#if defined(MT_IMPLEMENTATION) && !defined(_MT_IMPLEMENTATION_DONE)
#define _MT_IMPLEMENTATION_DONE 1

#include <string.h>

typedef struct mt__context {
    Dwfl_Module*    module;
    Dwarf_Die*      root;
    Dwfl*           lib;
} mt__context;

static mt_result mt__init_dwfl(Dwfl** /* out */ dwfl) {
    static char* debuginfo_path = NULL;

    static const Dwfl_Callbacks proc_callbacks = {
        .find_elf =         dwfl_linux_proc_find_elf,
        .find_debuginfo =   dwfl_standard_find_debuginfo,
        .section_address =  NULL,
        .debuginfo_path =   &debuginfo_path,
    };

    // @TEST: check if proc_callback can be NULL
    *dwfl = dwfl_begin(&proc_callbacks);
    if(!(*dwfl)) return MT_FAIL;

    // Report all mappings for the current running process
    if(dwfl_linux_proc_report(*dwfl, getpid()) != 0 || 
        dwfl_report_end(*dwfl, NULL, NULL) != 0) 
    {
        dwfl_end(*dwfl);
        return MT_FAIL;
    }

    return MT_OK;
}

static char* get_die_name(Dwarf_Die* die) {
    MT_ASSERT(die != NULL);
    
    Dwarf_Attribute attr;

    // 'dwarf_attr_integrate' :
    // Searches for a DIE attribute, following inheritance chains (DW_AT_abstract_origin
    // and DW_AT_specification) if not found directly in `die`.
    // Stores the resulting attribute descriptor in `*result` (&attr).
    // NULL signify missing or error so if that happens we just ignore it

    if(dwarf_attr_integrate(die, DW_AT_name, &attr)) /* if it is NULL we pass to the next one */ {
        const char* name = dwarf_formstring(&attr);
        
        if(name && name[0] != '\0') {
            return MT_STRDUP(name);
        }
    }

    if(dwarf_attr_integrate(die, DW_AT_linkage_name, &attr)) {
        const char* name = dwarf_formstring(&attr);
        
        if(name && name[0] != '\0') {
            return MT_STRDUP(name);
        }
    }

    if(dwarf_attr_integrate(die, DW_AT_MIPS_linkage_name, &attr)) {
        const char* name = dwarf_formstring(&attr);
        
        if(name && name[0] != '\0') {
            return MT_STRDUP(name);
        }
    }

    const char* dname = dwarf_diename(die);
    if(dname) {
        return MT_STRDUP(dname);
    }
    
    return NULL;
}


static mt_stack_trace _resolve_address_fallback(
    const mt__context* ctx, 
    mt_raw_address addr, 
    mt_raw_address pc,
    const char* mod_name)
{
    mt_stack_trace trace = {};
    trace.src_loc.line = MT_INVALID_LINE;
    trace.src_loc.col =  MT_INVALID_COL;

    trace.address =     addr;
    trace.tag =         MT_TAG_SUBPROGRAM;
    trace.mod_name =    mod_name ? MT_STRDUP(mod_name) : NULL;
    
    if(ctx->module != NULL) {
        const char* func = dwfl_module_addrname(ctx->module, pc);
        if(func != NULL) {
            trace.func_sign = MT_STRDUP(func);
        }
    }

    Dwfl_Line* line = dwfl_getsrc(ctx->lib, pc);
    if(line != NULL) {
        // @NOTE if dwfl_lineinfo fails the program crash
        trace.src_loc.file_path = MT_STRDUP(dwfl_lineinfo(line, NULL, 
                                                    &trace.src_loc.line, 
                                                    &trace.src_loc.col, 
                                                    NULL, NULL));
    }

    return trace;
}

static mt_stack_trace _resolve_address_die(
    const mt__context* ctx,
    Dwarf_Die* scope,
    mt_raw_address addr, 
    mt_raw_address pc,
    const char* mod_name) 
{
    mt_stack_trace trace = { };

    trace.address =       addr;
    trace.tag =           dwarf_tag(scope);
    trace.mod_name =      mod_name ? MT_STRDUP(mod_name) : NULL;
    trace.func_sign =     get_die_name(scope);
    trace.src_loc.line =  MT_INVALID_LINE;
    trace.src_loc.col =   MT_INVALID_COL;

    Dwfl_Line* line = dwfl_getsrc(ctx->lib, pc);
    if(line != NULL) {
        trace.src_loc.file_path = MT_STRDUP(dwfl_lineinfo(line, NULL, 
                                                    &trace.src_loc.line, 
                                                    &trace.src_loc.col, 
                                                    NULL, NULL));
    }

    return trace;
}

static mt_result mt__get_die_loc(mt_trace_loc* loc, Dwarf_Die* die, Dwarf_Files* files) {
    Dwarf_Attribute attr;
    Dwarf_Word call_file_idx, call_line, call_col;
    
    if(dwarf_attr(die, DW_AT_call_file, &attr)) {
        dwarf_formudata(&attr, &call_file_idx);
        const char* srcfile = dwarf_filesrc(files, call_file_idx, 
                                            NULL, NULL);
        if(srcfile) {
            loc->file_path = MT_STRDUP(srcfile);
        }
    }

    if(dwarf_attr(die, DW_AT_call_line, &attr)) {
        dwarf_formudata(&attr, &call_line);
        loc->line = (int32_t)call_line;
    }

    if(dwarf_attr(die, DW_AT_call_column, &attr)) {
        dwarf_formudata(&attr, &call_col);
        loc->col = (int32_t)call_col;
    }

    return MT_OK;
}

static mt_result mt__resolve_address(mt_stack_traces* /* out */ out, 
                                     Dwfl* lib, mt_raw_address addr) 
{
    mt_result result = MT_OK;

    // Return addresses point after the call instruction;
    // subtract 1 to fall within the calling instruction and its 
    // respective DWARF inline scope.
    mt_raw_address pc_adjusted = (addr > 0) ? (addr - 1) : addr;

    mt__context ctx = { };
    ctx.lib =         lib;

    // locating the module (executable, DSO)
    ctx.module = dwfl_addrmodule(ctx.lib, pc_adjusted);

    const char* mod_name =      NULL;

    // bias is the offset between runtime memory addresses and the addresses recorded
    // inside the binary's DWARF info (Bias = Runtime_Load_Address - DWARF_Base_Address).
    Dwarf_Addr bias = 0;

    Dwarf_Die* scopes = NULL;
    int nscopes = 0;

    if(ctx.module != NULL) {
        // extracts metadata and layout boundaries from a loaded Dwfl_Module.
        mod_name = dwfl_module_info(ctx.module, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        // 'dwfl_module_addrdie':
        // finds the DWARF Debugging Information Entry (DIE) of the 
        // Compilation Unit (CU) that contains a given runtime memory 
        // address within a module.
        // 
        // DWARF records function ranges, line numbers, and lexical scopes using static, 
        // un-relocated link-time virtual addresses (typically base 0 for PIE executables 
        // and shared libraries (.so)).
        // 
        // At runtime, ASLR or the dynamic linker loads the object at an arbitrary base 
        // address in memory. Therefore:
        //     DWARF_Address = Runtime_PC - bias
        ctx.root = dwfl_module_addrdie(ctx.module, pc_adjusted, &bias);

        if(ctx.root != NULL) {
            // Retrieves enclosing scopes for a DIE, ordered innermost to CU (index 0..N-1).
            // Allocates `*scopes`; THE CALLER IS RESPONSIBLE FOR FREEING THIS BUFFER USING `free()`.
            // Returns number of scopes (> 0), 0 if none found, or -1 on error (check dwarf_errno()).
            // 
            // dwarf_getscopes (and dwarf_getscopes_die) operates strictly on DWARF-relative 
            // offsets, so you must subtract the bias to look up the correct DIE.
            nscopes = dwarf_getscopes(ctx.root, pc_adjusted - bias, &scopes);
        }
    }

    // Find the innermost function-like DIE (subprogram or inlined subroutine)
    Dwarf_Die* func_die = NULL;
    if(nscopes > 0) {
        for(int i = 0; i < nscopes; ++i) {
            // @TEST: look at what the function returns for the number of scopes

            // Returns the DWARF tag identifier (DW_TAG_*) representing the DIE's type/role.
            // Returns DW_TAG_invalid on error or if `die` is NULL.
            int tag = dwarf_tag(&scopes[i]);

            if(tag == MT_TAG_SUBPROGRAM || tag == MT_TAG_INLINE || tag == MT_TAG_ENTRY_POINT) {
                func_die = &scopes[i];
                break;
            }
        }
    }

    Dwarf_Die* die_scopes =         NULL;
    int ndie_scopes =               0;
    mt_stack_trace inner_trace =    { };
    Dwarf_Die*      caller =        NULL;
    Dwarf_Files*    files =         NULL;

    if(func_die == NULL) {
        // fallback when no DWARF function found
        mt_stack_trace trace = _resolve_address_fallback(&ctx, addr, pc_adjusted, mod_name);
        mt_da_append(out, trace);

        mt_defer(MT_OK);
    }

    // 'dwarf_getscopes_die':
    // Retrieves all ancestor scopes enclosing `die`, moving upward to the CU (index 0 is `die`, N-1 is CU).
    // Allocates `*scopes`; caller must release the buffer using free().
    // Unlike dwarf_getscopes (which resolves from a CU via PC/offset), this walks directly from an existing DIE.
    // Returns count of scopes (> 0), 0 if none found, or -1 on error (check dwarf_errno()).
    ndie_scopes = dwarf_getscopes_die(func_die, &die_scopes);
    if(ndie_scopes < 0) {
        if(scopes != NULL) {
            free(scopes);
        }

        return MT_FAIL;
    }

    if(ndie_scopes == 0) {
        mt_stack_trace trace = _resolve_address_fallback(&ctx, addr, pc_adjusted, mod_name);
        mt_da_append(out, trace);

        mt_defer(MT_OK);
    }

    //innermost function
    inner_trace = _resolve_address_die(&ctx, &die_scopes[0], 
                                                        addr, pc_adjusted, 
                                                        mod_name);
    mt_da_append(out, inner_trace);

    // Parent inlined scopes and the enclosing subprogram
    caller = &die_scopes[0];
    dwarf_getsrcfiles(ctx.root, &files, NULL);

    for(int i = 1; i < ndie_scopes; ++i) {
        Dwarf_Die* callee = &die_scopes[i];
        int stag = dwarf_tag(callee);
        if(stag != MT_TAG_INLINE && stag != MT_TAG_SUBPROGRAM && 
            stag != MT_TAG_ENTRY_POINT) {
            continue;
        }

        mt_stack_trace parent_trace = { };
        parent_trace.address = addr;
        parent_trace.tag = stag;
        parent_trace.mod_name = mod_name ? MT_STRDUP(mod_name) : NULL;
        parent_trace.func_sign = get_die_name(callee);
        parent_trace.src_loc.line = MT_INVALID_LINE;
        parent_trace.src_loc.col =  MT_INVALID_COL;

        if(files != NULL) {
            mt__get_die_loc(&parent_trace.src_loc, caller, files);
        }

        mt_da_append(out, parent_trace);

        if(stag == MT_TAG_SUBPROGRAM) {
            break;
        }

        caller = callee;
    }


    free(die_scopes);

defer:
    if(scopes != NULL) {
        free(scopes);
    }

    return result;
}

static void mt__print_trace_colored(const mt_stack_trace* trace, FILE* stream) {    
    // In multithreading we ensure that multiple call to fprintf 
    // are not overlaped between threads
    flockfile(stream);

    // ADDRESS
    if(trace->tag == MT_TAG_INLINE) {
        fprintf(stream, MT_ADDRESS_COLOR "     (inline)     " COLOR_RESET);
    } else {
        fprintf(stream, MT_ADDRESS_COLOR "0x%016lx" COLOR_RESET, trace->address);
    }

    fprintf(stream, " in ");

    // FUNCTION
    fprintf(stream, MT_FUNCTION_COLOR "%s" COLOR_RESET, trace->func_sign? trace->func_sign: "<unknown>");
    fprintf(stream, " at ");

    // FILE PATH or MODULE
    if(trace->src_loc.file_path) {
        fprintf(stream, MT_FILE_PATH_MODULE_COLOR "%s" COLOR_RESET, trace->src_loc.file_path);

        // COLUMN AND LINE
        fprintf(stream, ":");
        fprintf(stream, MT_COL_LINE_COLOR "%d" COLOR_RESET, trace->src_loc.line);
        fprintf(stream, ":");
        fprintf(stream, MT_COL_LINE_COLOR "%d" COLOR_RESET, trace->src_loc.col);
    } else /* either DSO or unkown */{
        // It is utterly useless printing the line and column of a dynamic object
        fprintf(stream, MT_FILE_PATH_MODULE_COLOR "%s" COLOR_RESET, trace->mod_name ? trace->mod_name : "??");
    }

    funlockfile(stream);
}

static void mt__print_trace_uncolored(const mt_stack_trace* trace, FILE* stream) {
    // In multithreading we ensure that multiple call to fprintf 
    // are not overlaped between threads
    flockfile(stream);

    // ADDRESS
    if(trace->tag == MT_TAG_INLINE) {
        // If this is an inline function we set the "address" with 
        // the tag (inline) instead
        fprintf(stream, "     (inline)     ");
    } else {
        fprintf(stream, "0x%016lx", trace->address);
    }

    fprintf(stream, " in ");

    // FUNCTION
    fprintf(stream, "%s", trace->func_sign ? trace->func_sign : "<unknown>");
    fprintf(stream, " at ");

    // file_path or mod_name
    // FILE PATH or MODULE
    if(trace->src_loc.file_path) {
        fprintf(stream, "%s", trace->src_loc.file_path);

        // COLUMN AND LINE
        fprintf(stream, ":%d:%d", trace->src_loc.line, trace->src_loc.col);
    } else {
        fprintf(stream, "%s", trace->mod_name ? trace->mod_name : "??");
    }

    funlockfile(stream);
}


mt_result mt_unwind_stack_(mt_stack_raw_traces* out, struct mt__stack_opts opts) {
    unw_cursor_t    cursor;
    unw_context_t   context;

    if(unw_getcontext(&context) != 0 || unw_init_local(&cursor, &context) != 0) {
        return MT_FAIL;
    }

    while(unw_step(&cursor) > 0 && opts.max_depth > 0) {        
        if(opts.skip > 0) {
            opts.skip--; continue;
        }

        unw_word_t ip;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);

        mt_da_append(out, (Dwarf_Addr)(ip));
        
        opts.max_depth--;
    }

    return MT_OK;
}


mt_result mt_get_current_stack_trace_(mt_stack_traces* out, struct mt__stack_opts opts) {
    mt_result result = MT_OK;
    opts.skip += 1;

    Dwfl* dwarf_lib = NULL;
    if(mt__init_dwfl(&dwarf_lib) != MT_OK) {
        return MT_FAIL;
    }

    mt_stack_raw_traces raw = { };
    if(mt_unwind_stack_(&raw, opts) != MT_OK) {
        mt_defer(MT_FAIL);
    }

    for(uint32_t i = 0; i < raw.len; ++i) {
        if(mt__resolve_address(out, dwarf_lib, raw.items[i]) != MT_OK) {
            mt_defer(MT_FAIL);
        }
    }

defer:
    dwfl_end(dwarf_lib);
    mt_da_free(&raw);
    return result;
}


static void mt__print_stack_traces(mt_stack_traces* traces, FILE* stream, bool force_color) {
    bool color = force_color || isatty(fileno(stream));
    mt_da_foreach(traces, {
        fprintf(stream, "#%-3zu", it_idx);
        if(color) {
            mt__print_trace_colored(it, stream);
        } else {
            mt__print_trace_uncolored(it, stream);
        }
        fprintf(stream, "\n");
    });
}


void mt_free_stack_traces(mt_stack_traces* traces) {
    mt_da_foreach(traces, {
        MT_FREE((void*)it->func_sign);
        it->func_sign = NULL;

        MT_FREE((void*)it->src_loc.file_path);
        it->src_loc.file_path = NULL;

        MT_FREE((void*)it->mod_name);
        it->mod_name = NULL;
    });

    mt_da_free(traces);
}


mt_result mt_print_current_stack_trace_(struct mt__stack_print_opts opts) {
    if(opts.stream == NULL) {
        return MT_FAIL;
    }

    mt_stack_traces traces = { };

    if(mt_get_current_stack_trace_(&traces, 
        (struct mt__stack_opts){opts.skip + 1, opts.max_depth}) != MT_OK) {
            return MT_FAIL;
    }

    fprintf(opts.stream, "Stack trace (most recent call first)\n");
    mt__print_stack_traces(&traces, opts.stream, opts.force_color);

    mt_free_stack_traces(&traces);

    return MT_OK;
}

/**
 * Portions of this library were taken from:
 * - nob.h: https://github.com/tsoding/nob.h/
 *
 * ------------------------------------------------------------------------------
 *
 * LIBRARY: nob.h 
 * GITREPO: https://github.com/tsoding/nob.h/ @v3.10.0
 * LICENSE: MIT  
 * 
 * Copyright (c) 2024 Alexey Kutepov
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#endif

/*
    Revision history:

    0.1.0 [2026-10-26] First minor release

    We are following https://semver.org/ so the version has a format MAJOR.MINOR.PATCH:
      - Modifying comments does not update the version.
      - PATCH is incremented in case of a bug fix or refactoring without touching the API.
      - MINOR is incremented when new functions and/or types are added in a way that does
        not break any existing user code. We want to do this in the majority of the situation.
        If we want to delete a certain function or type in favor of another one we should
        just add the new function/type and deprecate the old one in a backward compatible way
        and let them co-exist for a while.
      - MAJOR update should be just a periodic cleanup of the DEPRECATED functions and types
        without really modifying any existing functionality.
      - Breaking backward compatibility in a MINOR release should be considered a bug and
        should be promptly fixed in the next PATCH release.
*/