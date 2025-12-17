#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

#include "platform.h"

char **platform_traverse_directory(const char *dir_path, size_t *children_count) {
    (void)dir_path;
    (void)children_count;

    assert(false && "TODO: Implement 'traverse_directory' for Windows");
}

bool platform_is_rebuild_needed(const char *output_path, const char **input_paths,
                       size_t input_paths_len) {
    (void)output_path;
    (void)input_paths;
    (void)input_paths_len;

    assert(false && "TODO: Implement 'is_rebuild_needed' for Windows");
}

bool platform_execute_command(const char **argv) {
    (void)argv;

    assert(false && "TODO: Implement 'execute_command' for Windows");
}
