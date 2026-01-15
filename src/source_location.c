#include "source_location.h"

SourceLocation source_location_of(const char *file_path, const char *buffer,
                                  uint32_t start) {
    SourceLocation loc = {file_path, 1, 1};

    for (uint32_t i = 0; i < start; i++) {
        if (buffer[i] == '\n') {
            loc.line += 1;
            loc.column = 1;
        } else {
            loc.column += 1;
        }
    }

    return loc;
}
