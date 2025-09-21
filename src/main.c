#include <stdio.h>

#ifndef RELEASE_MODE
#include "rebuild.c"
#endif // RELEASE_MODE

#define SHIFT(array_len, array_ptr) ((array_len)--, *(array_ptr)++)

static void usage(const char *program) {
    fprintf(stderr, "usage: %s <command> [options..] [arguments..]\n\n",
            program);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "\trun <input_file_path> - execute the specified file\n");
#ifndef RELEASE_MODE
    fprintf(stderr, "\trelease               - optimize the executable "
                    "and disable rebuilding\n");
#endif
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv) {
#ifndef RELEASE_MODE
    rebuild_if_needed(argv);
#endif // RELEASE_MODE

    const char *program = SHIFT(argc, argv);

    if (argc == 0) {
        usage(program);
        fprintf(stderr, "error: command was not provided\n");

        return 1;
    }

    const char *command = SHIFT(argc, argv);

    if (strcmp(command, "run") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        const char *input_file_path = SHIFT(argc, argv);

        printf("todo: execute %s\n", input_file_path);
#ifndef RELEASE_MODE
    } else if (strcmp(command, "release") == 0) {
        rebuild_in_release_mode(program);
#endif // RELEASE_MODE
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
