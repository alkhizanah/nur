#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "fs.h"

bool file_exists(const char *file_path) {
#if _WIN32
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(file_path, F_OK) == 0;
#endif
}

char *read_entire_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        fprintf(stderr, "error: could not open file '%s': %s\n", file_path,
                strerror(errno));

        exit(1);
    }

    char c;

    fread(&c, 1, 1, file);

    if (ferror(file)) {
        fprintf(stderr, "error: could not read file '%s': %s\n", file_path,
                strerror(errno));

        fclose(file);

        exit(1);
    }

    if (fseek(file, 0, SEEK_END) == -1) {
        fprintf(stderr, "error: could not get size of file '%s': %s\n",
                file_path, strerror(errno));

        fclose(file);

        exit(1);
    }

    long file_size = ftell(file);

    if (file_size == -1) {
        fprintf(stderr, "error: could not get size of file '%s': %s\n",
                file_path, strerror(errno));

        fclose(file);

        exit(1);
    }

    rewind(file);

    char *buffer = malloc(sizeof(char) * (file_size + 1));

    if (buffer == NULL) {
        fprintf(stderr, "error: out of memory\n");

        fclose(file);

        exit(1);
    }

    fread(buffer, 1, file_size, file);

    if (ferror(file)) {
        fprintf(stderr, "error: could not read file '%s': %s\n", file_path,
                strerror(errno));

        fclose(file);

        exit(1);
    }

    buffer[file_size] = 0;

    return buffer;
}
