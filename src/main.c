#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHIFT(array_len, array_ptr) ((array_len)--, *(array_ptr)++)

static inline void check_oom(void *object_ptr) {
    if (object_ptr == nullptr) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }
}

#ifndef RELEASE_MODE
static bool needs_rebuild(const char *output_path, const char **input_paths,
                          size_t input_paths_len) {
    struct stat stat_buf = {};

    if (stat(output_path, &stat_buf) != 0) {
        fprintf(stderr, "error: could not check %s statistics: %s\n",
                output_path, strerror(errno));

        exit(1);
    }

    auto output_path_timestamp = stat_buf.st_mtime;

    while (input_paths_len != 0) {
        auto input_path = SHIFT(input_paths_len, input_paths);

        if (stat(input_path, &stat_buf) != 0) {
            fprintf(stderr, "error: could not check %s statistics: %s\n",
                    input_path, strerror(errno));

            exit(1);
        }

        auto input_path_timestamp = stat_buf.st_mtime;

        if (input_path_timestamp > output_path_timestamp) {
            return true;
        }
    }

    return false;
}

static void rebuild(const char **argv) {
    auto src_dir = opendir("src");

    size_t input_paths_capacity = 0;

    struct dirent *src_entry = nullptr;

    while ((src_entry = readdir(src_dir))) {
        input_paths_capacity++;
    }

    rewinddir(src_dir);

    char **input_paths = malloc(input_paths_capacity * sizeof(*input_paths));

    check_oom(input_paths);

    size_t input_paths_len = 0;

    while ((src_entry = readdir(src_dir))) {
        auto input_path = &input_paths[input_paths_len++];

        *input_path = malloc((4 + 256) * sizeof(char));

        check_oom(*input_path);

        strncpy(*input_path, "src/", 4);
        strncpy(*input_path + 4, src_entry->d_name, 256);
    }

    closedir(src_dir);

    if (needs_rebuild(argv[0], (const char **)input_paths, input_paths_len)) {
        remove(argv[0]);

        while (input_paths_len > 0) {
            free(input_paths[--input_paths_len]);
        }

        free(input_paths);

        pid_t pid = fork();

        if (pid == -1) {
            fprintf(stderr, "error: could not fork to build the executable");

            exit(1);
        } else if (pid == 0) {
            execlp("clang", "clang", "-o", argv[0], "-std=c23", "-Wall",
                   "-Wextra", "src/main.c", NULL);
        } else {
            wait(NULL);

            execvp(argv[0], (char *const *)argv);
        }
    } else {
        while (input_paths_len > 0) {
            free(input_paths[--input_paths_len]);
        }

        free(input_paths);
    }
}
#endif // RELEASE_MODE

static void usage(const char *program) {
    fprintf(stderr, "usage: %s <command> [options..] [arguments..]\n\n",
            program);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "\trun <input_file_path> - execute the specified file\n");
#ifndef RELEASE_MODE
    fprintf(stderr, "\trelease               - build the executable again in "
                    "release mode\n");
#endif
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv) {
#ifndef RELEASE_MODE
    rebuild(argv);
#endif // RELEASE_MODE

    auto program = SHIFT(argc, argv);

    if (argc == 0) {
        usage(program);
        fprintf(stderr, "error: command was not provided\n");

        return 1;
    }

    auto command = SHIFT(argc, argv);

    if (strcmp(command, "run") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        auto input_file_path = SHIFT(argc, argv);

        printf("todo: execute %s\n", input_file_path);
#ifndef RELEASE_MODE
    } else if (strcmp(command, "release") == 0) {
        remove(program);

        pid_t pid = fork();

        if (pid == -1) {
            fprintf(stderr, "error: could not fork to build the executable");

            exit(1);
        } else if (pid == 0) {
            execlp("clang", "clang", "-o", program, "-std=c23", "-Wall",
                   "-Wextra", "-O3", "-DRELEASE_MODE", "src/main.c", NULL);
        } else {
            waitpid(pid, NULL, WEXITED);

            printf("ok: rebuilt the executable in release mode, rebuilding "
                   "capabilities are disabled now\n");
        }
#endif // RELEASE_MODE
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
