#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "interpreter.h"
#include "platform.h"

static void usage(const char *program) {
    fprintf(stderr, "usage: %s <command> [options..] [arguments..]\n\n",
            program);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "\trun <input_file_path> - execute the specified file\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv) {
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

        char *input_file_content = platform_read_entire_file(input_file_path);

        if (!interpret(input_file_path, input_file_content)) {
            return 1;
        }

        free(input_file_content);
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
