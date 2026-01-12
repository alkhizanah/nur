#pragma once

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_INIT_CAPACITY 256

#define ARRAY_SHIFT(array_count, array_ptr) ((array_count)--, *(array_ptr)++)

#define ARRAY_PUSH(arr, item)                                                  \
    do {                                                                       \
        if ((arr)->count + 1 > (arr)->capacity) {                                \
            size_t new_cap =                                                   \
                (arr)->capacity ? (arr)->capacity * 2 : ARRAY_INIT_CAPACITY;   \
                                                                               \
            (arr)->items =                                                     \
                realloc((arr)->items, sizeof(*(arr)->items) * new_cap);        \
                                                                               \
            if ((arr)->items == NULL) {                                        \
                fprintf(stderr, "error: out of memory\n");                     \
                                                                               \
                exit(1);                                                       \
            }                                                                  \
                                                                               \
            (arr)->capacity = new_cap;                                         \
        }                                                                      \
                                                                               \
        (arr)->items[(arr)->count++] = (item);                                   \
    } while (0)

#define ARRAY_FREE(arr)                                                        \
    do {                                                                       \
        free((arr)->items);                                                    \
        (arr)->items = NULL;                                                   \
        (arr)->count = 0;                                                        \
        (arr)->capacity = 0;                                                   \
    } while (0)

#define ARRAY_EXPAND(arr, src_items, src_count)                                  \
    do {                                                                       \
        size_t need = (arr)->count + (src_count);                                  \
                                                                               \
        if (need > (arr)->capacity) {                                          \
            size_t new_cap = (arr)->capacity ? (arr)->capacity : 4;            \
                                                                               \
            while (new_cap < need)                                             \
                new_cap *= 2;                                                  \
                                                                               \
            (arr)->items =                                                     \
                realloc((arr)->items, sizeof(*(arr)->items) * new_cap);        \
                                                                               \
            if (!(arr)->items) {                                               \
                fprintf(stderr, "error: out of memory\n");                     \
                exit(1);                                                       \
            }                                                                  \
                                                                               \
            (arr)->capacity = new_cap;                                         \
        }                                                                      \
                                                                               \
        memcpy((arr)->items + (arr)->count, (src_items),                         \
               sizeof(*(arr)->items) * (src_count));                             \
                                                                               \
        (arr)->count += (src_count);                                               \
    } while (0)
