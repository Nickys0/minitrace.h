/// FILE: run_tests.c
/// BRIEF: test runner for minitrace
/// COMMAND: cc -o run_tests run_tests.c

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

static const char* test_files[] = {
    "minitest.c",
    "minitest_multithreading.c",
    "minitest_with_inline.c",
};
static const size_t test_files_count = (sizeof(test_files) / sizeof(test_files[0]));

#define defer_with_value(_val)      \
        do{                         \
            result = (_val);        \
            goto defer;             \
        } while(0)

typedef struct string {
    char* items;
    size_t len;
    size_t size;
} string;

typedef struct string_view {
    const char* items;
    size_t len;
} string_view;

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

#define HEADER_COLOR YELLOW

#define SVLIT(literal) (string_view){ .items = (literal), .len = (sizeof((literal)) - 1) }
#define SV(it, l) (string_view){ .items = (it), .len = (l) }

#define OK   0
#define FAIL 1

#define print_colored_section(color, ...)       \
        do{                                     \
            bool istty = isatty(STDOUT_FILENO); \
            if(istty) {                         \
                fprintf(stdout, color);         \
                fflush(stdout);                 \
            }                                   \
            __VA_ARGS__                         \
            if(istty) {                         \
                fprintf(stdout, "\x1b[0m");     \
                fflush(stdout);                 \
            }                                   \
        } while(0)

int read_entire_file(string* out, const char* path) {
    FILE* file = fopen(path, "rb");
    
    if(file == NULL) {
        fprintf(stderr, "unable to open file '%s': %s\n", path, strerror(errno));
        return FAIL;
    }

    int result = 0;
    out->size = 1 * 1024 * 1024;
    out->items =  calloc(out->size, sizeof(char));

    long long m =       0;

    if(file == NULL) {
        defer_with_value(FAIL);
    }

    if(fseek(file, 0, SEEK_END) < 0) {
        defer_with_value(FAIL);
    }

    if((m = ftell(file)) < 0) {
        defer_with_value(FAIL);
    }

    if(fseek(file, 0, SEEK_SET) < 0){
        defer_with_value(FAIL);
    }

    out->len = (size_t)m;
    
    fread(out->items, m, 1, file);
    if(ferror(file)) {
        result = FAIL;
    }

defer:
    if(result != OK) {
        fprintf(stderr, "could not read file: %s\n", strerror(errno));
        if(out->items != NULL) {
            free(out->items);
            memset(out, 0, sizeof(*out));
        }
    }

    if(file != NULL){
        // After this fd is invalidated
        fclose(file);
    }

    return result;
}

string_view find_after_needle_in_haystack(const char* haystack, string_view needle, char sep) {
    const char* found =    strstr(haystack, needle.items);
    assert(found != NULL);

    found += needle.len;

    const char* end_of_ =  strchr(found,   sep);
    const size_t len =     end_of_ - found;

    return SV(found, len);
}

int run_test(size_t index) {
    int result = OK;
    string content = {0};

    if(read_entire_file(&content, test_files[index]) != OK) {
        return FAIL;
    }

    string_view command = find_after_needle_in_haystack(content.items, SVLIT("/// COMMAND: "), '\n');
    string_view output =  find_after_needle_in_haystack(content.items, SVLIT("-o "), ' ');

    // fprintf(stderr, "\n");
    // fprintf(stderr, "==================================================\n");
    // fprintf(stderr, "FROM FILE: %s\n",    test_files[index]);
    // fprintf(stderr, "COMMAND: |%.*s|\n",  (int)command.len, command.items);
    // fprintf(stderr, "OUTPUT: |%.*s|\n",   (int)output.len,  output.items);
    // fprintf(stderr, "==================================================\n");
    // fprintf(stderr, "\n");

    char command_buffer[1024] = {0};
    strncpy(command_buffer, command.items, command.len);

    print_colored_section(HEADER_COLOR, {
        fprintf(stdout, "\n");
        fprintf(stdout, "===========================================================================\n");
        fprintf(stdout, "=== COMPILING + RUNNING TEST '%s'\n", test_files[index]);
        fprintf(stdout, "===========================================================================\n");
        fprintf(stdout, "\n");
    });

    if(system(command_buffer) < 0) {
        fprintf(stderr, "unable to run command '%s': %s", command_buffer, strerror(errno));
        defer_with_value(FAIL);
    }

    sprintf(command_buffer, "./%.*s", (int)output.len, output.items);
    int r;
    if((r = system(command_buffer)) < 0) {
        fprintf(stderr, "unable to run command '%s': %s", command_buffer, strerror(errno));
        defer_with_value(FAIL);
    }

    print_colored_section(HEADER_COLOR, {
        fprintf(stdout, "\n");
        fprintf(stdout, "===========================================================================\n");
        fprintf(stdout, "=== END TEST '%s' (%s)\n", test_files[index], r != 0? "FAILED" : "OK");
        fprintf(stdout, "===========================================================================\n");
        fprintf(stdout, "\n");
    });


defer:
    free(content.items);    
    return result;
}

int main(void) {
    
    for(size_t i = 0; i < test_files_count; ++i) {
        run_test(i);
    }

    return 0;
}