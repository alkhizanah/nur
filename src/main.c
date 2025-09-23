#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "array.h"

#include "os.c"

static void usage(const char *program) {
    fprintf(stderr, "usage: %s <command> [options..] [arguments..]\n\n",
            program);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "\trun <input_file_path> - execute the specified file\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv) {
#ifndef NO_REBUILD
    size_t input_paths_len;

    char **input_paths = traverse_directory("src", &input_paths_len);

    if (is_rebuild_needed(argv[0], (const char **)input_paths,
                          input_paths_len)) {
        if (!execute_command((const char *[]){"clang", "-o", argv[0],
                                              "-std=c23", "-Wall", "-Wextra",
                                              "src/main.c", NULL})) {
            fprintf(stderr, "error: could not rebuild the executable\n");

            return 1;
        }

        execute_command(argv);

        return 0;
    } else {
        while (--input_paths_len) {
            free(input_paths[input_paths_len]);
        }

        free(input_paths);
    }
#endif

    const char *program = ARRAY_SHIFT(argc, argv);

    if (argc == 0) {
        usage(program);
        fprintf(stderr, "error: command was not provided\n");

        return 1;
    }

    const char *command = ARRAY_SHIFT(argc, argv);

    if (strcmp(command, "run") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        const char *input_file_path = ARRAY_SHIFT(argc, argv);

        printf("todo: execute %s\n", input_file_path);
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
