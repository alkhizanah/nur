#include <stdarg.h>
#include <stdio.h>

#include "diagnoser.h"

void diagnoser_error(SourceLocation loc, const char *format, ...) {
    va_list args;

    va_start(args, format);

    fprintf(stderr, "%s:%u:%u: error: ", loc.file_path, loc.line, loc.column);
    vfprintf(stderr, format, args);

    va_end(args);
}
