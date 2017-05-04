#pragma once

#include "mmap.h"

#include <stdbool.h>

bool
encoding_equal(const encoding *x, const encoding *y);

bool
encoding_equal_z(const encoding *x, const encoding *y);
