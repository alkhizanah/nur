#include "source_location.h"

SourceLocation source_location_of(const char *file_path, const char *buffer,
                                  Range range) {
    SourceLocation loc = {file_path, 1, 1};

    for (const char *end = buffer + range.start; buffer < end; buffer++) {
        if (*buffer == '\n') {
            loc.line += 1;
            loc.column = 1;
        } else {
            loc.column += 1;
        }
    }

    return loc;
}
