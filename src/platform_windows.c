#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

#include "platform.h"

bool platform_execute_command(const char **argv) {
    (void)argv;

    assert(false && "TODO: Implement 'execute_command' for Windows");
}
