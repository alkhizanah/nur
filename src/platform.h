#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Read an entire file into a heap allocated chunk of null-terminated bytes
char *platform_read_entire_file(const char *file_path);

/// Execute a command and return whether it was successful or not
bool platform_execute_command(const char **argv);
