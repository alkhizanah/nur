#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "array.h"
#include "os.h"

char **traverse_directory(const char *dir_path, size_t *children_count) {
    size_t dir_path_len = strlen(dir_path);

    DIR *dir = opendir(dir_path);

    *children_count = 0;

    struct dirent *entry = NULL;

    while ((entry = readdir(dir))) {
        // Skip "." and ".."
        if (*entry->d_name == '.' &&
            (entry->d_name[1] == 0 || entry->d_name[1] == '.')) {
            continue;
        }

        (*children_count)++;
    }

    rewinddir(dir);

    char **children_paths = malloc(*children_count * sizeof(*children_paths));

    if (children_paths == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    struct stat stat_buf;

    size_t i = 0;

    while ((entry = readdir(dir))) {
        // Skip "." and ".." again
        if (*entry->d_name == '.' &&
            (entry->d_name[1] == 0 || entry->d_name[1] == '.')) {
            continue;
        }

        // Construct the relative path (for example: "src/posix.c", instead of
        // "posix.c")
        char *relative_path = malloc((dir_path_len + 1 + 256) * sizeof(char));

        if (relative_path == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }

        strncpy(relative_path, dir_path, dir_path_len);
        relative_path[dir_path_len] = '/';
        strncpy(relative_path + dir_path_len + 1, entry->d_name, 256);

        if (stat(relative_path, &stat_buf) != 0) {
            fprintf(stderr, "error: could not stat %s: %s\n", entry->d_name,
                    strerror(errno));

            exit(1);
        }

        children_paths[i++] = relative_path;

        // If it is a directory, traverse it recursivly and then concatenate it
        // into our list
        if (S_ISDIR(stat_buf.st_mode)) {
            size_t other_paths_len;

            char **other_paths =
                traverse_directory(relative_path, &other_paths_len);

            *children_count += other_paths_len;

            children_paths = realloc(children_paths,
                                     *children_count * sizeof(*children_paths));

            for (size_t j = 0; j < other_paths_len;)
                children_paths[i++] = other_paths[j++];

            free(other_paths);
        }
    }

    closedir(dir);

    return children_paths;
}

bool is_rebuild_needed(const char *output_path, const char **input_paths,
                       size_t input_paths_len) {
    struct stat stat_buf = {};

    if (stat(output_path, &stat_buf) != 0) {
        fprintf(stderr, "error: could not stat %s: %s\n", output_path,
                strerror(errno));

        exit(1);
    }

    time_t output_path_timestamp = stat_buf.st_mtime;

    while (input_paths_len != 0) {
        const char *input_path = ARRAY_SHIFT(input_paths_len, input_paths);

        if (stat(input_path, &stat_buf) != 0) {
            fprintf(stderr, "error: could not stat %s: %s\n", input_path,
                    strerror(errno));

            exit(1);
        }

        time_t input_path_timestamp = stat_buf.st_mtime;

        if (input_path_timestamp > output_path_timestamp) {
            return true;
        }
    }

    return false;
}

bool execute_command(const char **argv) {
    pid_t pid = fork();

    switch (pid) {
    case -1:
        return false;
    case 0:
        execvp(argv[0], (char *const *)argv);

        // The exec() functions return only if an error has occured
        return false;
    default: {
        int wstatus;

        if (waitpid(pid, &wstatus, 0) == -1) {
            return false;
        };

        return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
    }
    }
}
