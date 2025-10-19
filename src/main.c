#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array.h"

#include "lexer.c"
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

    bool should_rebuild =
        is_rebuild_needed(argv[0], (const char **)input_paths, input_paths_len);

    while (--input_paths_len) {
        free(input_paths[input_paths_len]);
    }

    free(input_paths);

    if (should_rebuild) {
        if (!execute_command((const char *[]){"cc", "-o", argv[0], "-Wall",
                                              "-Wextra", "src/main.c", NULL})) {
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

        // NOTE(alsakandari): The input file content lives until our program
        // dies, it should not be freed at all, this is not a memory leak since
        // we use it frequently
        char *input_file_content = read_entire_file(input_file_path);

        Lexer lexer = {.buffer = input_file_content};

        Token token;

        while ((token = lexer_next(&lexer)).tag != TOK_EOF) {
            if (token.tag == TOK_INVALID) {
                printf("INVALID TOKEN: %.*s\n",
                       (int)(token.range.end - token.range.start),
                       input_file_content + token.range.start);
            } else {
                printf("TOKEN(%d): %.*s\n", token.tag,
                       (int)(token.range.end - token.range.start),
                       input_file_content + token.range.start);
            }
        }
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
