#pragma once

#define ARRAY_SHIFT(array_len, array_ptr) ((array_len)--, *(array_ptr)++)
