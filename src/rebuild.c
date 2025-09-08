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

#include "macros.h"
#include "oom.h"

bool is_rebuild_needed(const char *output_path, const char **input_paths,
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

void rebuild_if_needed(const char **argv) {
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

    if (is_rebuild_needed(argv[0], (const char **)input_paths, input_paths_len)) {
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
                   "-Wextra", "src/main.c", nullptr);
        } else {
            wait(nullptr);

            execvp(argv[0], (char *const *)argv);
        }
    } else {
        while (input_paths_len > 0) {
            free(input_paths[--input_paths_len]);
        }

        free(input_paths);
    }
}

void rebuild_in_release_mode(const char *program) {
    pid_t pid = fork();

    if (pid == -1) {
        fprintf(stderr, "error: could not fork to build the executable");

        exit(1);
    } else if (pid == 0) {
        execlp("clang", "clang", "-o", program, "-std=c23", "-Wall", "-Wextra",
               "-O3", "-DRELEASE_MODE", "src/main.c", nullptr);
    } else {
        wait(nullptr);

        printf("ok: rebuilt the executable in release mode, rebuilding "
               "capabilities are disabled now\n");
    }
}
