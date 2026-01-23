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
#include "platform.h"

bool platform_execute_command(const char **argv) {
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
