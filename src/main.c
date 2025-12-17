#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "ast.h"
#include "lexer.h"
#include "os.h"

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

    bool should_rebuild =
        is_rebuild_needed(argv[0], (const char **)input_paths, input_paths_len);

    for (size_t i = 0; i < input_paths_len; i++) {
        free(input_paths[i]);
    }

    free(input_paths);

    if (should_rebuild) {
        if (!execute_command(
                (const char *[]){"cc", "-o", argv[0], "src/one.c", NULL})) {
            fprintf(stderr, "error: could not rebuild the executable\n");

            return 1;
        }

        execute_command(argv);

        return 0;
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

        char *input_file_content = read_entire_file(input_file_path);

        AstParser parser = {.file_path = input_file_path,
                            .lexer = {.buffer = input_file_content}};

        if (!ast_parse(&parser)) {
            return 1;
        }

        free(input_file_content);
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
