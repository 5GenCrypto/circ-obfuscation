#pragma once

#include "../mmap.h"

const pp_vtable *
get_pp_vtable(const mmap_vtable *const mmap);

const sp_vtable *
get_sp_vtable(const mmap_vtable *const mmap);

const encoding_vtable *
get_encoding_vtable(const mmap_vtable *const mmap);
