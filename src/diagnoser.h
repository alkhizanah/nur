#pragma once

#include "source_location.h"

[[gnu::format(printf, 2, 3)]]
void diagnoser_error(SourceLocation, const char *format, ...);
