#include <stdio.h>
#include <stdlib.h>

#include "oom.h"

inline void check_oom(void *object_ptr) {
    if (object_ptr == nullptr) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }
}
