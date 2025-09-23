#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Traverse the directory recursively and collect the relative path of each
/// child
char **traverse_directory(const char *dir_path, size_t *children_count);

/// Check whether a rebuild of a program is needed, by checking if one of its
/// source files are modified after the program's compilation
bool is_rebuild_needed(const char *output_path, const char **input_paths,
                       size_t input_paths_len);

/// Execute a command and return whether it was successful or not
bool execute_command(const char **argv);
