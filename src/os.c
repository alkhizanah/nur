#ifdef _WIN32
#include "windows.c"
#else
#include "posix.c"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "os.h"

char *read_entire_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        fprintf(stderr, "error: could not open file '%s': %s\n", file_path,
                strerror(errno));

        exit(1);
    }

    // NOTE(alsakandari): The reason we do this is to check whether we can read
    // from it or not, then we calculate the size of the file, then we read from
    // it again, I made it like that after seeing an issue where Linux
    // allows you to `fopen` a directory, using `ftell` on it gave me a huge
    // number which in turn makes an out of memory failure instead of indicating
    // it was a directory
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
