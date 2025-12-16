#pragma once

#include <stdint.h>

#include "range.h"

typedef struct {
    const char *file_path;
    uint32_t line;
    uint32_t column;
} SourceLocation;

SourceLocation source_location_of(const char *file_path, const char *buffer,
                                  Range);
